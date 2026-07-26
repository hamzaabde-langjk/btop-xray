#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <dlfcn.h>
#include <time.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "engine/adapter.h"
#include "gfx/renderer.h"
#include "ui/interface.h"
#include "shared/shm.h"

#define VERSION "1.0"
#define DEFAULT_SHM_KEY 0x58415259

static volatile bool running = true;
static SharedMemoryHeader *shm = NULL;
static EngineAdapter *engine = NULL;
static UIInterface *ui = NULL;
static Renderer *renderer = NULL;
static Display *display = NULL;
static Window window = 0;
static int detail_mode = 0;

static void signal_handler(int sig) {
    (void)sig;
    running = false;
    printf("\nShutting down...\n");
}

static void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  XRAY-SCOPE v1.0 - System Monitor                              ║\n");
    printf("║  Live Process Monitoring with Resizeable GUI                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
}

static EngineAdapter* detect_engine(void) {
    printf("Loading engine...\n");
    void *handle = dlopen("./polling.so", RTLD_NOW);
    if (!handle) handle = dlopen("polling.so", RTLD_NOW);
    if (handle) {
        EngineAdapter *adapter = (EngineAdapter*)dlsym(handle, "adapter");
        if (adapter && adapter->init()) {
            printf("Engine ready\n");
            return adapter;
        }
        dlclose(handle);
    }
    fprintf(stderr, "Failed to load engine\n");
    return NULL;
}

static void* collect_data_thread(void *arg) {
    (void)arg;
    int counter = 0;
    while (running && engine) {
        ProcessInfo procs[512];
        int proc_count = engine->get_processes(procs, 512);
        if (proc_count > 0) {
            while (atomic_flag_test_and_set(&shm->lock));
            shm->process_count = proc_count;
            memcpy(shm->processes, procs, proc_count * sizeof(ProcessInfo));
            shm->num_processes = proc_count;
            shm->timestamp = time(NULL);
            atomic_flag_clear(&shm->lock);
            if (counter % 50 == 0 && proc_count > 0) {
                printf("Collected %d processes (first: %s, CPU: %ld%%)\n",
                       proc_count, procs[0].name, procs[0].cpu_usage);
            }
            counter++;
        }
        usleep(100000);
    }
    return NULL;
}

static void handle_x11_events(void) {
    if (!display || !ui) return;
    XEvent ev;
    while (XPending(display) > 0) {
        XNextEvent(display, &ev);
        if (ev.type == KeyPress) {
            KeySym key = XLookupKeysym(&ev.xkey, 0);
            if (key == XK_q || key == XK_Q || key == XK_Escape) {
                running = false;
            }
            if (key == XK_d || key == XK_D) {
                detail_mode = !detail_mode;
                printf("Detail mode: %s\n", detail_mode ? "ON" : "OFF");
            }
            if (key == XK_Up) {
                SimpleUI *sui = (SimpleUI*)ui->data;
                if (sui->scroll_offset > 0) {
                    sui->scroll_offset--;
                }
            }
            if (key == XK_Down) {
                SimpleUI *sui = (SimpleUI*)ui->data;
                if (shm && sui->scroll_offset + 20 < shm->process_count) {
                    sui->scroll_offset++;
                }
            }
        }
        /* RESIZE EVENT - Table adjusts automatically */
        if (ev.type == ConfigureNotify) {
            SimpleUI *sui = (SimpleUI*)ui->data;
            sui->width = ev.xconfigure.width;
            sui->height = ev.xconfigure.height;
            if (ui->render) ui->render(ui->data);
        }
        if (ev.type == Expose) {
            if (ui && ui->render) ui->render(ui->data);
        }
        /* CLICK TO SELECT */
        if (ev.type == ButtonPress) {
            SimpleUI *sui = (SimpleUI*)ui->data;
            int y = ev.xbutton.y;
            int table_y = sui->header_height + 42;
            int row_h = sui->row_height;
            int idx = (y - table_y - 20) / row_h + sui->scroll_offset;
            if (shm && idx >= 0 && idx < shm->process_count) {
                sui->selected_pid = shm->processes[idx].pid;
                printf("Selected PID: %d\n", sui->selected_pid);
            }
        }
    }
}

int main(int argc, char **argv) {
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = true;
    }
    
    print_banner();
    printf("Mode: %s\n", headless ? "HEADLESS" : "GUI");
    printf("\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    shm = shm_init(DEFAULT_SHM_KEY, sizeof(SharedMemoryHeader));
    if (!shm) {
        fprintf(stderr, "Shared memory failed\n");
        return 1;
    }
    memset(shm, 0, sizeof(SharedMemoryHeader));
    shm->magic = 0x58415259;
    shm->version = 1;
    shm->process_count = 0;
    shm->event_count = 0;
    atomic_flag_clear(&shm->lock);
    
    engine = detect_engine();
    if (!engine) {
        fprintf(stderr, "Engine failed\n");
        return 1;
    }
    
    if (!headless) {
        renderer = renderer_create(true, false);
        if (renderer) {
            ui = ui_create(renderer, shm, NULL);
            if (ui) {
                display = XOpenDisplay(NULL);
                if (display) {
                    window = DefaultRootWindow(display);
                }
            }
        }
        if (!ui) {
            printf("GUI failed, running in headless mode\n");
            headless = true;
        }
    }
    
    pthread_t collector;
    if (pthread_create(&collector, NULL, collect_data_thread, NULL) != 0) {
        fprintf(stderr, "Thread failed\n");
        return 1;
    }
    
    printf("Running... (Press Ctrl+C or 'q' to stop)\n");
    printf("Controls: Up/Down scroll, D=details, click to select, resize window\n\n");
    sleep(2);
    
    while (running) {
        if (headless) {
            sleep(2);
            while (atomic_flag_test_and_set(&shm->lock));
            printf("\rProcesses: %d | Events: %d   ",
                   shm->process_count, shm->event_count);
            fflush(stdout);
            atomic_flag_clear(&shm->lock);
        } else {
            if (ui && ui->render) ui->render(ui->data);
            if (display) handle_x11_events();
            usleep(50000);
        }
    }
    
    running = false;
    pthread_join(collector, NULL);
    if (ui) ui_destroy(ui);
    if (renderer) renderer_destroy(renderer);
    if (engine) engine->cleanup();
    if (shm) shm_destroy(shm);
    if (display) XCloseDisplay(display);
    
    printf("\nXRAY-SCOPE stopped successfully\n");
    return 0;
}

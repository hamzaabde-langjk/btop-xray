#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <time.h>
#include "interface.h"
#include "../shared/shm.h"

#define BG_COLOR     0x0D1117
#define HEADER_COLOR 0x0A0E14
#define CARD_COLOR   0x1C2333
#define ACCENT       0x58A6FF
#define GREEN        0x3FB950
#define ORANGE       0xF0883E
#define RED          0xF85149
#define TEXT         0xE6EDF3
#define TEXT_SEC     0x8B949E
#define BORDER       0x30363D

static void draw_rect(UIState *ui, int x, int y, int w, int h, unsigned long color) {
    if (!ui || !ui->dpy || !ui->gc) return;
    XSetForeground(ui->dpy, ui->gc, color);
    XFillRectangle(ui->dpy, ui->win, ui->gc, x, y, w, h);
}

static void draw_text(UIState *ui, int x, int y, const char *text, unsigned long color) {
    if (!ui || !ui->dpy || !ui->gc || !text) return;
    XSetForeground(ui->dpy, ui->gc, color);
    XDrawString(ui->dpy, ui->win, ui->gc, x, y, text, strlen(text));
}

static void draw_progress(UIState *ui, int x, int y, int w, int h, int pct, unsigned long color) {
    if (!ui) return;
    draw_rect(ui, x, y, w, h, 0x2D3748);
    if (pct > 0) {
        int fw = (w * pct) / 100;
        if (fw < 1) fw = 1;
        draw_rect(ui, x, y, fw, h, color);
    }
}

static void render_ui(void *ptr) {
    if (!ptr) return;
    UIState *ui = (UIState*)ptr;
    if (!ui->dpy || !ui->win) return;
    
    SharedMemoryHeader *shm = (SharedMemoryHeader*)ui->data;
    
    int p = ui->padding;
    int rh = ui->row_height;
    int hh = ui->header_height;
    int fh = ui->footer_height;
    
    draw_rect(ui, 0, 0, ui->width, ui->height, BG_COLOR);
    draw_rect(ui, 0, 0, ui->width, hh, HEADER_COLOR);
    draw_rect(ui, 0, hh-2, ui->width, 2, ACCENT);
    
    draw_text(ui, p+5, hh-10, "XRAY-SCOPE v1.0", 0xFFFFFF);
    
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
    draw_text(ui, ui->width-80, hh-10, ts, TEXT_SEC);
    draw_text(ui, ui->width-150, hh-10, "Q:quit  Up/Down:Scroll", TEXT_SEC);
    
    int sy = hh + 4;
    draw_rect(ui, p, sy, ui->width-2*p, 28, CARD_COLOR);
    
    if (shm) {
        char info[128];
        snprintf(info, sizeof(info), "Processes: %d  Scroll: %d",
                 shm->process_count, ui->scroll_offset);
        draw_text(ui, p+10, sy+20, info, TEXT);
    } else {
        draw_text(ui, p+10, sy+20, "Waiting...", TEXT_SEC);
    }
    
    int ty = sy + 36;
    int th = ui->height - ty - fh - 8;
    if (th < 50) th = 50;
    
    draw_rect(ui, p, ty, ui->width-2*p, th, CARD_COLOR);
    
    int hy = ty + 16;
    draw_text(ui, p+15, hy, "PID", ACCENT);
    draw_text(ui, p+80, hy, "PROCESS", ACCENT);
    draw_text(ui, p+210, hy, "CPU%", ACCENT);
    draw_text(ui, p+270, hy, "MEM(MB)", GREEN);
    draw_text(ui, p+330, hy, "STATE", ACCENT);
    draw_text(ui, p+390, hy, "COMMAND", ACCENT);
    
    draw_rect(ui, p+10, hy+6, ui->width-2*p-20, 1, BORDER);
    
    if (!shm || shm->process_count == 0) {
        draw_text(ui, p+15, hy+30, "No processes", TEXT_SEC);
        XFlush(ui->dpy);
        return;
    }
    
    int cnt = shm->process_count;
    if (cnt > 4096) cnt = 4096;
    
    ProcessInfo sorted[4096];
    memcpy(sorted, shm->processes, cnt * sizeof(ProcessInfo));
    
    for (int i = 0; i < cnt-1; i++) {
        for (int j = 0; j < cnt-i-1; j++) {
            if (sorted[j].memory_usage < sorted[j+1].memory_usage) {
                ProcessInfo tmp = sorted[j];
                sorted[j] = sorted[j+1];
                sorted[j+1] = tmp;
            }
        }
    }
    
    int max_display = 20;
    int max_scroll = cnt - max_display;
    if (max_scroll < 0) max_scroll = 0;
    if (ui->scroll_offset > max_scroll) ui->scroll_offset = max_scroll;
    if (ui->scroll_offset < 0) ui->scroll_offset = 0;
    
    int yp = hy + 20;
    int rw = ui->width - 2*p - 20;
    
    for (int i = ui->scroll_offset; i < cnt && (i - ui->scroll_offset) < max_display; i++) {
        ProcessInfo *proc = &sorted[i];
        
        unsigned long rc = (i - ui->scroll_offset) % 2 == 0 ? CARD_COLOR : 0x11161D;
        draw_rect(ui, p+10, yp-12, rw, rh, rc);
        
        unsigned long tc = TEXT;
        if (proc->pid == ui->selected_pid) {
            draw_rect(ui, p+10, yp-12, rw, rh, 0x1A3A5C);
            tc = 0xFFFFFF;
        }
        
        char buf[64];
        snprintf(buf, sizeof(buf), "%d", proc->pid);
        draw_text(ui, p+15, yp, buf, tc);
        
        char name[21];
        strncpy(name, proc->name, 20);
        name[20] = '\0';
        draw_text(ui, p+80, yp, name, tc);
        
        int cpu = proc->cpu_usage;
        if (cpu > 100) cpu = 100;
        draw_progress(ui, p+212, yp-7, 45, 10, cpu, ACCENT);
        snprintf(buf, sizeof(buf), "%3d%%", cpu);
        draw_text(ui, p+262, yp, buf, ACCENT);
        
        long mem = proc->memory_usage / 1024;
        unsigned long mc = GREEN;
        if (mem > 1000) mc = ORANGE;
        if (mem > 5000) mc = RED;
        snprintf(buf, sizeof(buf), "%5ld", mem);
        draw_text(ui, p+270, yp, buf, mc);
        
        char st[5];
        unsigned long sc = TEXT_SEC;
        switch(proc->state) {
            case 'R': strcpy(st, "RUN"); sc = GREEN; break;
            case 'S': strcpy(st, "SLP"); sc = ACCENT; break;
            case 'Z': strcpy(st, "ZMB"); sc = ORANGE; break;
            case 'T': strcpy(st, "STP"); sc = RED; break;
            default: snprintf(st, 5, "%c", proc->state); break;
        }
        draw_text(ui, p+330, yp, st, sc);
        
        char cmd[21];
        strncpy(cmd, proc->cmdline, 19);
        cmd[19] = '\0';
        draw_text(ui, p+390, yp, cmd, TEXT_SEC);
        
        yp += rh;
    }
    
    int fy = ui->height - fh;
    draw_rect(ui, 0, fy, ui->width, fh, 0x161B22);
    draw_rect(ui, 0, fy, ui->width, 1, BORDER);
    
    char footer[256];
    int disp = ui->scroll_offset + max_display;
    if (disp > cnt) disp = cnt;
    snprintf(footer, sizeof(footer),
             "Total: %d | Showing: %d-%d | Q:Quit Up/Down:Scroll",
             cnt, ui->scroll_offset + 1, disp);
    draw_text(ui, p+10, fy+18, footer, TEXT_SEC);
    
    XFlush(ui->dpy);
}

static void destroy_ui(void *ptr) {
    if (!ptr) return;
    UIState *ui = (UIState*)ptr;
    if (ui->gc) XFreeGC(ui->dpy, ui->gc);
    if (ui->win) XDestroyWindow(ui->dpy, ui->win);
    if (ui->dpy) XCloseDisplay(ui->dpy);
    free(ui);
}

UIInterface* ui_create(void *shm_data) {
    UIState *ui = calloc(1, sizeof(UIState));
    if (!ui) return NULL;
    
    ui->width = 700;
    ui->height = 550;
    ui->data = shm_data;
    ui->selected_pid = -1;
    ui->scroll_offset = 0;
    ui->header_height = 40;
    ui->footer_height = 50;
    ui->row_height = 24;
    ui->padding = 10;
    
    ui->dpy = XOpenDisplay(NULL);
    if (!ui->dpy) {
        free(ui);
        return NULL;
    }
    
    int screen = DefaultScreen(ui->dpy);
    Window root = RootWindow(ui->dpy, screen);
    
    XSetWindowAttributes attrs;
    attrs.background_pixel = BG_COLOR;
    attrs.border_pixel = BORDER;
    attrs.event_mask = KeyPressMask | ExposureMask | StructureNotifyMask | ButtonPressMask;
    
    ui->win = XCreateWindow(ui->dpy, root, 0, 0,
                            ui->width, ui->height, 2,
                            CopyFromParent, InputOutput, CopyFromParent,
                            CWBackPixel | CWBorderPixel | CWEventMask, &attrs);
    
    XStoreName(ui->dpy, ui->win, "XRAY-SCOPE");
    XMapWindow(ui->dpy, ui->win);
    
    ui->gc = XCreateGC(ui->dpy, ui->win, 0, NULL);
    
    XEvent ev;
    int timeout = 100;
    while (timeout-- > 0) {
        if (XCheckTypedWindowEvent(ui->dpy, ui->win, MapNotify, &ev)) {
            break;
        }
        usleep(10000);
    }
    
    UIInterface *iface = malloc(sizeof(UIInterface));
    if (!iface) {
        destroy_ui(ui);
        return NULL;
    }
    
    iface->render = render_ui;
    iface->destroy = destroy_ui;
    iface->state = ui;
    
    printf("GUI ready\n");
    return iface;
}

void ui_destroy(UIInterface *ui) {
    if (!ui) return;
    if (ui->destroy) {
        ui->destroy(ui->state);
    }
    free(ui);
}

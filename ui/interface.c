#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include "interface.h"
#include "../shared/shm.h"

/* Color Scheme - Professional Dark Theme */
#define COLOR_BG          0x0D1117
#define COLOR_BG_SECOND   0x161B22
#define COLOR_BG_CARD     0x1C2333
#define COLOR_BG_HEADER   0x0A0E14
#define COLOR_ACCENT      0x58A6FF
#define COLOR_ACCENT_GREEN 0x3FB950
#define COLOR_ACCENT_ORANGE 0xF0883E
#define COLOR_ACCENT_RED  0xF85149
#define COLOR_TEXT        0xE6EDF3
#define COLOR_TEXT_SEC    0x8B949E
#define COLOR_TEXT_BRIGHT 0xFFFFFF
#define COLOR_BORDER      0x30363D
#define COLOR_CPU         0x58A6FF
#define COLOR_MEM         0x3FB950
#define COLOR_RUNNING     0x3FB950
#define COLOR_SLEEPING    0x58A6FF
#define COLOR_ZOMBIE      0xF0883E
#define COLOR_STOPPED     0xF85149
#define COLOR_DISK        0xD2A8FF
#define COLOR_ROW_ALT     0x11161D
#define COLOR_SELECTED    0x1A3A5C

/* Helper Functions */
static void draw_rectangle(SimpleUI *ui, int x, int y, int w, int h, unsigned long color) {
    if (!ui || !ui->display || !ui->gc) return;
    XSetForeground(ui->display, ui->gc, color);
    XFillRectangle(ui->display, ui->window, ui->gc, x, y, w, h);
}

static void draw_rounded_rect(SimpleUI *ui, int x, int y, int w, int h, int r, unsigned long color) {
    if (!ui || !ui->display || !ui->gc) return;
    if (w < 2*r || h < 2*r) {
        draw_rectangle(ui, x, y, w, h, color);
        return;
    }
    XSetForeground(ui->display, ui->gc, color);
    XFillRectangle(ui->display, ui->window, ui->gc, x + r, y, w - 2*r, h);
    XFillRectangle(ui->display, ui->window, ui->gc, x, y + r, w, h - 2*r);
    XFillArc(ui->display, ui->window, ui->gc, x, y, 2*r, 2*r, 0, 360*64);
    XFillArc(ui->display, ui->window, ui->gc, x + w - 2*r, y, 2*r, 2*r, 0, 360*64);
    XFillArc(ui->display, ui->window, ui->gc, x, y + h - 2*r, 2*r, 2*r, 0, 360*64);
    XFillArc(ui->display, ui->window, ui->gc, x + w - 2*r, y + h - 2*r, 2*r, 2*r, 0, 360*64);
}

static void draw_text(SimpleUI *ui, int x, int y, const char *text, unsigned long color) {
    if (!ui || !ui->display || !ui->gc || !text) return;
    XSetForeground(ui->display, ui->gc, color);
    XDrawString(ui->display, ui->window, ui->gc, x, y, text, strlen(text));
}

static void draw_text_bold(SimpleUI *ui, int x, int y, const char *text, unsigned long color) {
    if (!ui || !ui->display || !ui->gc || !text) return;
    XSetForeground(ui->display, ui->gc, color);
    if (ui->font_bold) {
        XSetFont(ui->display, ui->gc, ui->font_bold->fid);
    }
    XDrawString(ui->display, ui->window, ui->gc, x, y, text, strlen(text));
    if (ui->font) {
        XSetFont(ui->display, ui->gc, ui->font->fid);
    }
}

static void draw_progress_bar(SimpleUI *ui, int x, int y, int w, int h, int percent, unsigned long color) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    draw_rounded_rect(ui, x, y, w, h, 3, 0x2D3748);
    if (percent > 0) {
        int fill_w = (w * percent) / 100;
        if (fill_w < 1) fill_w = 1;
        draw_rounded_rect(ui, x, y, fill_w, h, 3, color);
    }
}

static void draw_horizontal_line(SimpleUI *ui, int x, int y, int w, unsigned long color) {
    if (!ui || !ui->display || !ui->gc) return;
    XSetForeground(ui->display, ui->gc, color);
    XFillRectangle(ui->display, ui->window, ui->gc, x, y, w, 1);
}

/* Main Render Function */
void ui_render(void *data) {
    if (!data) return;
    SimpleUI *ui = (SimpleUI*)data;
    if (!ui->display || !ui->window_ready) { usleep(10000); return; }
    
    int p = ui->padding;
    int rh = ui->row_height;
    int header_h = ui->header_height;
    int footer_h = ui->footer_height;
    
    /* Clear Background */
    draw_rectangle(ui, 0, 0, ui->width, ui->height, COLOR_BG);
    
    /* === HEADER === */
    draw_rounded_rect(ui, 0, 0, ui->width, header_h, 0, COLOR_BG_HEADER);
    draw_horizontal_line(ui, 0, header_h - 2, ui->width, COLOR_ACCENT);
    
    /* Title */
    draw_text_bold(ui, p + 5, header_h - 14, "XRAY-SCOPE", COLOR_TEXT_BRIGHT);
    draw_text(ui, p + 130, header_h - 14, "v1.0", COLOR_TEXT_SEC);
    
    /* Time */
    time_t now = time(NULL);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
    draw_text(ui, ui->width - 80, header_h - 14, time_str, COLOR_TEXT_SEC);
    
    /* Controls */
    draw_text(ui, ui->width - 190, header_h - 14, "Q:quit  D:details", COLOR_TEXT_SEC);
    
    /* === STATS BAR === */
    int stats_y = header_h + 4;
    draw_rounded_rect(ui, p, stats_y, ui->width - 2*p, 30, 4, COLOR_BG_CARD);
    
    SharedMemoryHeader *shm = (SharedMemoryHeader*)ui->shm_data;
    if (shm) {
        char info[128];
        snprintf(info, sizeof(info), "Processes: %d  |  Events: %d",
                 shm->process_count, shm->event_count);
        draw_text(ui, p + 12, stats_y + 20, info, COLOR_TEXT);
        
        long total_cpu = 0;
        int max_check = shm->process_count < 100 ? shm->process_count : 100;
        for (int i = 0; i < max_check; i++) {
            total_cpu += shm->processes[i].cpu_usage;
        }
        char cpu_info[32];
        snprintf(cpu_info, sizeof(cpu_info), "CPU: %ld%%", total_cpu);
        draw_text(ui, ui->width - p - 120, stats_y + 20, cpu_info, COLOR_ACCENT);
        
        draw_text(ui, ui->width - p - 55, stats_y + 20, "MEM:", COLOR_TEXT_SEC);
        long total_mem = 0;
        for (int i = 0; i < max_check; i++) {
            total_mem += shm->processes[i].memory_usage / 1024;
        }
        char mem_info[24];
        snprintf(mem_info, sizeof(mem_info), "%ld MB", total_mem);
        draw_text(ui, ui->width - p - 15, stats_y + 20, mem_info, COLOR_ACCENT_GREEN);
    } else {
        draw_text(ui, p + 12, stats_y + 20, "Waiting for data...", COLOR_TEXT_SEC);
    }
    
    /* === TABLE === */
    int table_y = stats_y + 38;
    int table_h = ui->height - table_y - footer_h - 8;
    draw_rounded_rect(ui, p, table_y, ui->width - 2*p, table_h, 6, COLOR_BG_CARD);
    
    /* Table Headers */
    int header_y = table_y + 18;
    int col_pid = p + 18;
    int col_user = p + 78;
    int col_name = p + 150;
    int col_cpu = p + 280;
    int col_mem = p + 340;
    int col_state = p + 400;
    int col_threads = p + 460;
    int col_cmd = p + 530;
    
    draw_text_bold(ui, col_pid, header_y, "PID", COLOR_ACCENT);
    draw_text_bold(ui, col_user, header_y, "USER", COLOR_ACCENT);
    draw_text_bold(ui, col_name, header_y, "PROCESS", COLOR_ACCENT);
    draw_text_bold(ui, col_cpu, header_y, "CPU", COLOR_ACCENT);
    draw_text_bold(ui, col_mem, header_y, "MEM", COLOR_ACCENT);
    draw_text_bold(ui, col_state, header_y, "STATE", COLOR_ACCENT);
    draw_text_bold(ui, col_threads, header_y, "THREADS", COLOR_ACCENT);
    draw_text_bold(ui, col_cmd, header_y, "COMMAND", COLOR_ACCENT);
    
    draw_horizontal_line(ui, p + 12, header_y + 6, ui->width - 2*p - 24, COLOR_BORDER);
    
    if (!shm || shm->process_count == 0) {
        draw_text(ui, p + 18, header_y + 35, "No processes found", COLOR_TEXT_SEC);
        XFlush(ui->display);
        return;
    }
    
    /* Sort processes by CPU */
    int count = shm->process_count;
    if (count > 4096) count = 4096;
    
    ProcessInfo sorted[4096];
    memcpy(sorted, shm->processes, count * sizeof(ProcessInfo));
    
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (sorted[j].cpu_usage < sorted[j+1].cpu_usage) {
                ProcessInfo temp = sorted[j];
                sorted[j] = sorted[j+1];
                sorted[j+1] = temp;
            }
        }
    }
    
    /* Calculate visible rows - ADJUSTS TO WINDOW SIZE */
    int max_display = (table_h - 40) / rh;
    if (max_display < 1) max_display = 1;
    if (max_display > 50) max_display = 50;
    
    int y_pos = header_y + 20;
    int row_width = ui->width - 2*p - 24;
    
    for (int i = ui->scroll_offset; i < count && (i - ui->scroll_offset) < max_display; i++) {
        ProcessInfo *proc = &sorted[i];
        
        /* Row background */
        unsigned long row_color = (i - ui->scroll_offset) % 2 == 0 ? COLOR_BG_CARD : COLOR_ROW_ALT;
        draw_rectangle(ui, p + 12, y_pos - 12, row_width, rh, row_color);
        
        /* Selected highlight */
        unsigned long text_color = COLOR_TEXT;
        if (proc->pid == ui->selected_pid) {
            draw_rounded_rect(ui, p + 12, y_pos - 12, row_width, rh, 3, COLOR_SELECTED);
            text_color = COLOR_TEXT_BRIGHT;
        }
        
        char buffer[64];
        
        /* PID */
        snprintf(buffer, sizeof(buffer), "%d", proc->pid);
        draw_text(ui, col_pid, y_pos, buffer, text_color);
        
        /* User */
        char user[9];
        strncpy(user, proc->user, 8);
        user[8] = '\0';
        draw_text(ui, col_user, y_pos, user, COLOR_TEXT_SEC);
        
        /* Process Name */
        char name[21];
        strncpy(name, proc->name, 20);
        name[20] = '\0';
        draw_text(ui, col_name, y_pos, name, text_color);
        
        /* CPU with bar */
        int cpu_pct = proc->cpu_usage;
        if (cpu_pct > 100) cpu_pct = 100;
        draw_progress_bar(ui, col_cpu + 2, y_pos - 7, 45, 10, cpu_pct, COLOR_CPU);
        snprintf(buffer, sizeof(buffer), "%3d%%", cpu_pct);
        draw_text(ui, col_cpu + 52, y_pos, buffer, COLOR_CPU);
        
        /* Memory */
        long mem_mb = proc->memory_usage / 1024;
        snprintf(buffer, sizeof(buffer), "%5ld", mem_mb);
        draw_text(ui, col_mem, y_pos, buffer, COLOR_ACCENT_GREEN);
        
        /* State with color */
        unsigned long state_color = COLOR_TEXT_SEC;
        char state_str[5];
        switch(proc->state) {
            case 'R': strcpy(state_str, "RUN"); state_color = COLOR_RUNNING; break;
            case 'S': strcpy(state_str, "SLP"); state_color = COLOR_SLEEPING; break;
            case 'D': strcpy(state_str, "DSK"); state_color = COLOR_DISK; break;
            case 'Z': strcpy(state_str, "ZMB"); state_color = COLOR_ZOMBIE; break;
            case 'T': strcpy(state_str, "STP"); state_color = COLOR_STOPPED; break;
            default: snprintf(state_str, 5, "%c", proc->state); break;
        }
        draw_text(ui, col_state, y_pos, state_str, state_color);
        
        /* Threads */
        snprintf(buffer, sizeof(buffer), "%4ld", proc->num_threads);
        draw_text(ui, col_threads, y_pos, buffer, COLOR_TEXT_SEC);
        
        /* Command (truncated) */
        char cmd[31];
        strncpy(cmd, proc->cmdline, 29);
        cmd[29] = '\0';
        draw_text(ui, col_cmd, y_pos, cmd, COLOR_TEXT_SEC);
        
        y_pos += rh;
    }
    
    /* === FOOTER === */
    int footer_y = ui->height - footer_h;
    draw_rectangle(ui, 0, footer_y, ui->width, footer_h, COLOR_BG_SECOND);
    draw_horizontal_line(ui, 0, footer_y, ui->width, COLOR_BORDER);
    
    char footer[256];
    int displayed = ui->scroll_offset + max_display;
    if (displayed > count) displayed = count;
    snprintf(footer, sizeof(footer),
             "Total: %d processes  |  Showing: %d-%d  |  Q: Quit  D: Details  Up/Down: Scroll",
             count, ui->scroll_offset + 1, displayed);
    draw_text(ui, p + 10, footer_y + 18, footer, COLOR_TEXT_SEC);
    
    XFlush(ui->display);
}

/* UI Creation and Destruction */
static void ui_destroy_internal(void *data) {
    if (!data) return;
    SimpleUI *ui = (SimpleUI*)data;
    if (ui->font) XFreeFont(ui->display, ui->font);
    if (ui->font_bold) XFreeFont(ui->display, ui->font_bold);
    if (ui->gc) XFreeGC(ui->display, ui->gc);
    if (ui->window) XDestroyWindow(ui->display, ui->window);
    if (ui->display) XCloseDisplay(ui->display);
    free(ui);
}

UIInterface* ui_create(void *renderer, void *shm_data, const char *filter) {
    (void)renderer;
    (void)filter;
    
    SimpleUI *ui = malloc(sizeof(SimpleUI));
    if (!ui) return NULL;
    
    memset(ui, 0, sizeof(SimpleUI));
    ui->width = 850;
    ui->height = 650;
    ui->shm_data = shm_data;
    ui->running = true;
    ui->selected_pid = -1;
    ui->window_ready = 0;
    ui->scroll_offset = 0;
    ui->detail_mode = 0;
    ui->header_height = 44;
    ui->footer_height = 60;
    ui->row_height = 26;
    ui->padding = 12;
    ui->corner_radius = 6;
    ui->font_height = 14;
    
    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        free(ui);
        return NULL;
    }
    
    /* Load fonts */
    ui->font = XLoadQueryFont(ui->display, "fixed");
    ui->font_bold = XLoadQueryFont(ui->display, "fixed");
    if (ui->font) {
        ui->font_height = ui->font->ascent + ui->font->descent;
    }
    
    int screen = DefaultScreen(ui->display);
    Window root = RootWindow(ui->display, screen);
    
    XSetWindowAttributes attrs;
    attrs.background_pixel = COLOR_BG;
    attrs.border_pixel = COLOR_BORDER;
    attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                       StructureNotifyMask | PointerMotionMask;
    
    ui->window = XCreateWindow(ui->display, root, 0, 0,
                               ui->width, ui->height, 2,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWBorderPixel | CWEventMask, &attrs);
    
    XStoreName(ui->display, ui->window, "XRAY-SCOPE - System Monitor");
    XMapWindow(ui->display, ui->window);
    
    ui->gc = XCreateGC(ui->display, ui->window, 0, NULL);
    if (ui->font) {
        XSetFont(ui->display, ui->gc, ui->font->fid);
    }
    
    /* Wait for window */
    XEvent ev;
    int timeout = 200;
    while (timeout-- > 0) {
        if (XCheckTypedWindowEvent(ui->display, ui->window, MapNotify, &ev)) {
            ui->window_ready = 1;
            break;
        }
        usleep(10000);
    }
    
    if (!ui->window_ready) {
        XMapRaised(ui->display, ui->window);
        XFlush(ui->display);
        ui->window_ready = 1;
    }
    
    UIInterface *interface = malloc(sizeof(UIInterface));
    if (!interface) {
        ui_destroy_internal(ui);
        return NULL;
    }
    
    interface->update = NULL;
    interface->render = ui_render;
    interface->destroy = ui_destroy_internal;
    interface->data = ui;
    
    printf("GUI Interface ready\n");
    return interface;
}

void ui_destroy(UIInterface *ui) {
    if (!ui) return;
    if (ui->destroy) {
        ui->destroy(ui->data);
    }
    free(ui);
}

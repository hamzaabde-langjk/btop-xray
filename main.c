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
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#define VERSION "1.0"
#define MAX_PROCESSES 4096
#define RADIUS 20

/* ============================================================
   COLORS
   ============================================================ */
#define COLOR_BG_DARK          0x0D1117
#define COLOR_BG_CARD_DARK     0x161B22
#define COLOR_BG_HEADER_DARK   0x0A0E14
#define COLOR_TEXT_DARK        0xE6EDF3
#define COLOR_TEXT_SEC_DARK    0x8B949E
#define COLOR_TEXT_BRIGHT_DARK 0xFFFFFF

#define COLOR_BG_LIGHT          0xFFFFFF
#define COLOR_BG_CARD_LIGHT     0xF0F0F0
#define COLOR_BG_HEADER_LIGHT   0xE8E8E8
#define COLOR_TEXT_LIGHT        0x1A1A1A
#define COLOR_TEXT_SEC_LIGHT    0x555555
#define COLOR_TEXT_BRIGHT_LIGHT 0x000000

#define COLOR_ACCENT      0x58A6FF
#define COLOR_ACCENT_GREEN 0x3FB950
#define COLOR_ACCENT_ORANGE 0xF0883E
#define COLOR_ACCENT_RED  0xF85149
#define COLOR_BORDER      0x30363D
#define COLOR_SCROLL_BG   0x1C2333
#define COLOR_SCROLL_THUMB 0x58A6FF
#define COLOR_SCROLL_THUMB_HOVER 0x79C0FF

/* ============================================================
   SETTINGS STRUCTURE
   ============================================================ */
typedef struct {
    int sleep_mode;
    int show_settings;
    int dark_mode;
} Settings;

static Settings settings = {
    .sleep_mode = 0,
    .show_settings = 0,
    .dark_mode = 1
};

static unsigned long COLOR_BG = COLOR_BG_DARK;
static unsigned long COLOR_BG_CARD = COLOR_BG_CARD_DARK;
static unsigned long COLOR_BG_HEADER = COLOR_BG_HEADER_DARK;
static unsigned long COLOR_TEXT = COLOR_TEXT_DARK;
static unsigned long COLOR_TEXT_SEC = COLOR_TEXT_SEC_DARK;
static unsigned long COLOR_TEXT_BRIGHT = COLOR_TEXT_BRIGHT_DARK;

/* ============================================================
   CPU CALCULATION
   ============================================================ */
typedef struct {
    unsigned long long total_time;
} CpuData;

static CpuData prev_cpu[MAX_PROCESSES];
static unsigned long long prev_system_total = 0;
static int first_run = 1;

/* ============================================================
   STRUCTURES
   ============================================================ */
typedef struct {
    char name[64];
    int pid;
    int ppid;
    char cmdline[512];
    char user[64];
    char state;
    long cpu_usage;
    long memory_usage;
    long num_threads;
} ProcessInfo;

typedef struct {
    ProcessInfo processes[MAX_PROCESSES];
    int count;
    time_t timestamp;
} ProcessData;

static ProcessData proc_data;
static int scroll_offset = 0;
static int selected_pid = -1;
static volatile bool running = true;
static int scrollbar_width = 22;
static int scrollbar_dragging = 0;
static int settings_hover = 0;

/* ============================================================
   READ SYSTEM CPU TIME ONCE
   ============================================================ */
static unsigned long long read_system_cpu_total(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
        fclose(f);
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }
    fclose(f);
    return 0;
}

/* ============================================================
   READ PROCESS CPU TIME
   ============================================================ */
static unsigned long long read_process_cpu_time(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    
    unsigned long long utime = 0, stime = 0;
    char buf[1024];
    if (fgets(buf, sizeof(buf), f)) {
        char *token = strtok(buf, " ");
        int field = 0;
        while (token && field < 15) {
            if (field == 13) utime = strtoull(token, NULL, 10);
            if (field == 14) stime = strtoull(token, NULL, 10);
            token = strtok(NULL, " ");
            field++;
        }
    }
    fclose(f);
    return utime + stime;
}

/* ============================================================
   READ PROCESS DATA
   ============================================================ */
static void read_process_data(void) {
    DIR *dir = opendir("/proc");
    if (!dir) return;
    
    struct dirent *entry;
    int count = 0;
    
    unsigned long long system_total = read_system_cpu_total();
    
    while ((entry = readdir(dir)) != NULL && count < MAX_PROCESSES) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        
        ProcessInfo *p = &proc_data.processes[count];
        memset(p, 0, sizeof(ProcessInfo));
        p->pid = pid;
        
        char path[256];
        
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        FILE *f = fopen(path, "r");
        if (f) {
            fgets(p->name, sizeof(p->name), f);
            char *nl = strchr(p->name, '\n');
            if (nl) *nl = '\0';
            fclose(f);
        } else {
            snprintf(p->name, sizeof(p->name), "pid_%d", pid);
        }
        
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        f = fopen(path, "r");
        if (f) {
            char buf[1024];
            if (fgets(buf, sizeof(buf), f)) {
                char *token = strtok(buf, " ");
                int field = 0;
                while (token && field < 3) {
                    if (field == 2) {
                        p->state = token[0];
                        break;
                    }
                    token = strtok(NULL, " ");
                    field++;
                }
            }
            fclose(f);
        }
        
        snprintf(path, sizeof(path), "/proc/%d/statm", pid);
        f = fopen(path, "r");
        if (f) {
            long resident;
            if (fscanf(f, "%*ld %ld", &resident) == 1) {
                p->memory_usage = resident * 4;
            }
            fclose(f);
        }
        
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        f = fopen(path, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    unsigned int uid;
                    sscanf(line, "Uid: %u", &uid);
                    struct passwd *pw = getpwuid(uid);
                    if (pw) {
                        strncpy(p->user, pw->pw_name, sizeof(p->user)-1);
                    } else {
                        snprintf(p->user, sizeof(p->user), "%u", uid);
                    }
                    break;
                }
            }
            fclose(f);
        }
        
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        f = fopen(path, "r");
        if (f) {
            if (fgets(p->cmdline, sizeof(p->cmdline)-1, f)) {
                for (int i = 0; p->cmdline[i]; i++) {
                    if (p->cmdline[i] == '\0') p->cmdline[i] = ' ';
                }
            }
            fclose(f);
        }
        
        unsigned long long process_total = read_process_cpu_time(pid);
        
        if (!first_run && prev_cpu[pid % MAX_PROCESSES].total_time > 0) {
            unsigned long long sys_diff = system_total - prev_system_total;
            unsigned long long proc_diff = process_total - prev_cpu[pid % MAX_PROCESSES].total_time;
            
            if (sys_diff > 0 && proc_diff > 0) {
                p->cpu_usage = (proc_diff * 100) / sys_diff;
                if (p->cpu_usage > 100) p->cpu_usage = 100;
            } else {
                p->cpu_usage = 0;
            }
        } else {
            p->cpu_usage = 0;
        }
        
        prev_cpu[pid % MAX_PROCESSES].total_time = process_total;
        
        p->num_threads = 1;
        count++;
    }
    
    closedir(dir);
    proc_data.count = count;
    proc_data.timestamp = time(NULL);
    
    prev_system_total = system_total;
    first_run = 0;
}

/* ============================================================
   SORT PROCESSES BY MEMORY
   ============================================================ */
static void sort_processes(ProcessInfo *sorted, int count) {
    if (!sorted || count <= 0) return;
    memcpy(sorted, proc_data.processes, count * sizeof(ProcessInfo));
    
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (sorted[j].memory_usage < sorted[j+1].memory_usage) {
                ProcessInfo tmp = sorted[j];
                sorted[j] = sorted[j+1];
                sorted[j+1] = tmp;
            }
        }
    }
}

/* ============================================================
   X11 GUI - EXTRA LARGE FONT (40px)
   ============================================================ */
static Display *dpy;
static Window win;
static GC gc;
static int win_width = 1400;
static int win_height = 900;
static XFontStruct *font;
static XFontStruct *font_bold;
static int font_height = 40;
static int row_height = 58;
static int settings_btn_x = 0;
static int settings_btn_y = 0;
static int settings_btn_w = 140;
static int settings_btn_h = 48;

static void update_theme(void) {
    if (settings.dark_mode) {
        COLOR_BG = COLOR_BG_DARK;
        COLOR_BG_CARD = COLOR_BG_CARD_DARK;
        COLOR_BG_HEADER = COLOR_BG_HEADER_DARK;
        COLOR_TEXT = COLOR_TEXT_DARK;
        COLOR_TEXT_SEC = COLOR_TEXT_SEC_DARK;
        COLOR_TEXT_BRIGHT = COLOR_TEXT_BRIGHT_DARK;
    } else {
        COLOR_BG = COLOR_BG_LIGHT;
        COLOR_BG_CARD = COLOR_BG_CARD_LIGHT;
        COLOR_BG_HEADER = COLOR_BG_HEADER_LIGHT;
        COLOR_TEXT = COLOR_TEXT_LIGHT;
        COLOR_TEXT_SEC = COLOR_TEXT_SEC_LIGHT;
        COLOR_TEXT_BRIGHT = COLOR_TEXT_BRIGHT_LIGHT;
    }
}

static void load_fonts(void) {
    /* Try 40px fonts */
    font = XLoadQueryFont(dpy, "-misc-dejavu sans mono-medium-r-normal--40-*-*-*-*-*-*-*");
    if (!font) font = XLoadQueryFont(dpy, "-misc-dejavu sans mono-medium-r-normal--36-*-*-*-*-*-*-*");
    if (!font) font = XLoadQueryFont(dpy, "-misc-dejavu sans mono-medium-r-normal--32-*-*-*-*-*-*-*");
    if (!font) font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--32-*-*-*-*-*-*-*");
    if (!font) font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--28-*-*-*-*-*-*-*");
    if (!font) font = XLoadQueryFont(dpy, "fixed");
    
    if (font) {
        font_height = font->ascent + font->descent;
        row_height = font_height + 22;
        XSetFont(dpy, gc, font->fid);
    }
    
    font_bold = XLoadQueryFont(dpy, "-misc-dejavu sans mono-bold-r-normal--40-*-*-*-*-*-*-*");
    if (!font_bold) font_bold = XLoadQueryFont(dpy, "-misc-dejavu sans mono-bold-r-normal--36-*-*-*-*-*-*-*");
    if (!font_bold) font_bold = font;
}

static void draw_rect(int x, int y, int w, int h, unsigned long color) {
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, win, gc, x, y, w, h);
}

static void draw_rounded_rect(int x, int y, int w, int h, unsigned long color) {
    int r = RADIUS;
    if (w < 2*r || h < 2*r) {
        draw_rect(x, y, w, h, color);
        return;
    }
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, win, gc, x + r, y, w - 2*r, h);
    XFillRectangle(dpy, win, gc, x, y + r, w, h - 2*r);
    XFillArc(dpy, win, gc, x, y, 2*r, 2*r, 0, 360*64);
    XFillArc(dpy, win, gc, x + w - 2*r, y, 2*r, 2*r, 0, 360*64);
    XFillArc(dpy, win, gc, x, y + h - 2*r, 2*r, 2*r, 0, 360*64);
    XFillArc(dpy, win, gc, x + w - 2*r, y + h - 2*r, 2*r, 2*r, 0, 360*64);
}

static void draw_rounded_rect_border(int x, int y, int w, int h, unsigned long color) {
    int r = RADIUS;
    if (w < 2*r || h < 2*r) {
        XSetForeground(dpy, gc, color);
        XDrawRectangle(dpy, win, gc, x, y, w, h);
        return;
    }
    XSetForeground(dpy, gc, color);
    XDrawLine(dpy, win, gc, x + r, y, x + w - r, y);
    XDrawLine(dpy, win, gc, x + r, y + h, x + w - r, y + h);
    XDrawLine(dpy, win, gc, x, y + r, x, y + h - r);
    XDrawLine(dpy, win, gc, x + w, y + r, x + w, y + h - r);
    XDrawArc(dpy, win, gc, x, y, 2*r, 2*r, 0, 360*64);
    XDrawArc(dpy, win, gc, x + w - 2*r, y, 2*r, 2*r, 0, 360*64);
    XDrawArc(dpy, win, gc, x, y + h - 2*r, 2*r, 2*r, 0, 360*64);
    XDrawArc(dpy, win, gc, x + w - 2*r, y + h - 2*r, 2*r, 2*r, 0, 360*64);
}

static void draw_text(int x, int y, const char *text, unsigned long color) {
    XSetForeground(dpy, gc, color);
    XDrawString(dpy, win, gc, x, y, text, strlen(text));
}

static void draw_text_bold(int x, int y, const char *text, unsigned long color) {
    if (font_bold && font_bold != font) {
        XSetFont(dpy, gc, font_bold->fid);
    }
    XSetForeground(dpy, gc, color);
    XDrawString(dpy, win, gc, x, y, text, strlen(text));
    if (font) {
        XSetFont(dpy, gc, font->fid);
    }
}

static void draw_progress(int x, int y, int w, int h, int pct, unsigned long color) {
    draw_rect(x, y, w, h, 0x2D3748);
    if (pct > 0) {
        int fw = (w * pct) / 100;
        if (fw < 1) fw = 1;
        draw_rect(x, y, fw, h, color);
    }
}

static void draw_scrollbar(int total_rows, int visible_rows) {
    if (total_rows <= visible_rows) return;
    
    int sb_x = win_width - scrollbar_width - 15;
    int sb_y = 180;
    int sb_h = win_height - 260;
    
    draw_rect(sb_x, sb_y, scrollbar_width, sb_h, COLOR_SCROLL_BG);
    draw_rect(sb_x, sb_y, scrollbar_width, sb_h, 0x30363D);
    
    float ratio = (float)visible_rows / total_rows;
    int thumb_h = sb_h * ratio;
    if (thumb_h < 40) thumb_h = 40;
    
    int max_scroll = total_rows - visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    float scroll_ratio = (max_scroll > 0) ? (float)scroll_offset / max_scroll : 0;
    int thumb_y = sb_y + (sb_h - thumb_h) * scroll_ratio;
    
    unsigned long color = COLOR_SCROLL_THUMB;
    if (scrollbar_dragging) color = COLOR_SCROLL_THUMB_HOVER;
    draw_rect(sb_x + 1, thumb_y, scrollbar_width - 2, thumb_h, color);
}

/* ============================================================
   SETTINGS WINDOW
   ============================================================ */
static void draw_settings_window(void) {
    int sw = 480, sh = 300;
    int sx = (win_width - sw) / 2;
    int sy = (win_height - sh) / 2;
    
    draw_rounded_rect(sx, sy, sw, sh, 0x1A2332);
    draw_rounded_rect_border(sx, sy, sw, sh, 0x30363D);
    draw_rect(sx+1, sy+1, sw-2, sh-2, 0x1A2332);
    
    draw_text_bold(sx+22, sy+48, "Settings", COLOR_ACCENT);
    draw_rect(sx+14, sy+66, sw-28, 1, COLOR_BORDER);
    
    int ypos = sy + 96;
    int label_x = sx + 22;
    int ctrl_x = sx + 200;
    int ctrl_w = 240;
    int row_h = 50;
    
    draw_text(label_x, ypos+20, "Sleep Mode:", COLOR_TEXT);
    unsigned long sleep_color = settings.sleep_mode ? 0x1A3A5C : 0x2D3748;
    draw_rounded_rect(ctrl_x, ypos, ctrl_w, row_h, sleep_color);
    draw_rounded_rect_border(ctrl_x, ypos, ctrl_w, row_h, 0x30363D);
    draw_text(ctrl_x+14, ypos+34, settings.sleep_mode ? "ON" : "OFF", 
              settings.sleep_mode ? COLOR_ACCENT_GREEN : COLOR_TEXT_SEC);
    ypos += row_h + 14;
    
    draw_text(label_x, ypos+20, "Theme:", COLOR_TEXT);
    unsigned long theme_color = settings.dark_mode ? 0x1A3A5C : 0x2D3748;
    draw_rounded_rect(ctrl_x, ypos, ctrl_w, row_h, theme_color);
    draw_rounded_rect_border(ctrl_x, ypos, ctrl_w, row_h, 0x30363D);
    draw_text(ctrl_x+14, ypos+34, settings.dark_mode ? "Dark" : "Light", 
              settings.dark_mode ? COLOR_ACCENT : COLOR_ACCENT_ORANGE);
    ypos += row_h + 14;
    
    draw_rounded_rect(sx + 100, sy + sh - 66, sw - 200, 48, COLOR_ACCENT);
    draw_text_bold(sx + sw/2 - 32, sy + sh - 34, "Close", COLOR_TEXT_BRIGHT);
}

/* ============================================================
   RENDER MAIN GUI
   ============================================================ */
static void render_gui(void) {
    if (!dpy || !win) return;
    
    int p = 24, hh = 80, fh = 78;
    int table_y = hh + 66;
    int table_h = win_height - table_y - fh - 16;
    if (table_h < 120) table_h = 120;
    
    draw_rect(0, 0, win_width, win_height, COLOR_BG);
    
    draw_rounded_rect(0, 0, win_width, hh, COLOR_BG_HEADER);
    draw_rect(0, hh-3, win_width, 3, COLOR_ACCENT);
    
    draw_text_bold(p+14, hh-22, "XRAY-SCOPE v1.0", COLOR_TEXT_BRIGHT);
    
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
    draw_text(win_width-160, hh-22, ts, COLOR_TEXT_SEC);
    
    settings_btn_x = win_width - 280;
    settings_btn_y = 18;
    settings_btn_w = 140;
    settings_btn_h = 46;
    
    unsigned long btn_color = settings_hover ? 0x2D3748 : 0x1C2333;
    draw_rounded_rect(settings_btn_x, settings_btn_y, settings_btn_w, settings_btn_h, btn_color);
    draw_rounded_rect_border(settings_btn_x, settings_btn_y, settings_btn_w, settings_btn_h, 0x30363D);
    draw_text(settings_btn_x + 22, settings_btn_y + 32, "Settings", COLOR_TEXT_SEC);
    
    int sy = hh + 10;
    draw_rounded_rect(p, sy, win_width-2*p-scrollbar_width-12, 54, COLOR_BG_CARD);
    draw_rounded_rect_border(p, sy, win_width-2*p-scrollbar_width-12, 54, 0x30363D);
    
    char info[128];
    char theme_name[8];
    strcpy(theme_name, settings.dark_mode ? "Dark" : "Light");
    
    char *user = getenv("USER");
    if (!user) user = getenv("LOGNAME");
    if (!user) user = "unknown";
    
    snprintf(info, sizeof(info), "Processes: %d  |  User: %s  |  Theme: %s  |  Sorted: MEMORY",
             proc_data.count, user, theme_name);
    draw_text(p+20, sy+38, info, COLOR_TEXT);
    
    draw_rounded_rect(p, table_y, win_width-2*p-scrollbar_width-12, table_h, COLOR_BG_CARD);
    draw_rounded_rect_border(p, table_y, win_width-2*p-scrollbar_width-12, table_h, 0x30363D);
    
    int hy = table_y + 34;
    draw_text_bold(p+24, hy, "PID", COLOR_ACCENT);
    draw_text_bold(p+140, hy, "USER", COLOR_ACCENT);
    draw_text_bold(p+260, hy, "PROCESS", COLOR_ACCENT);
    draw_text_bold(p+460, hy, "CPU%", COLOR_ACCENT);
    draw_text_bold(p+580, hy, "MEM(MB)", COLOR_ACCENT_GREEN);
    draw_text_bold(p+700, hy, "STATE", COLOR_ACCENT);
    draw_text_bold(p+820, hy, "COMMAND", COLOR_ACCENT);
    
    draw_rect(p+16, hy+14, win_width-2*p-scrollbar_width-38, 2, COLOR_BORDER);
    
    if (proc_data.count == 0) {
        draw_text(p+24, hy+64, "No processes found", COLOR_TEXT_SEC);
        XFlush(dpy);
        return;
    }
    
    ProcessInfo sorted[MAX_PROCESSES];
    sort_processes(sorted, proc_data.count);
    
    int cnt = proc_data.count;
    int max_display = (table_h - 68) / row_height;
    if (max_display < 1) max_display = 1;
    if (max_display > 25) max_display = 25;
    
    int max_scroll = cnt - max_display;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    if (scroll_offset < 0) scroll_offset = 0;
    
    draw_scrollbar(cnt, max_display);
    
    int yp = hy + 42;
    int rw = win_width - 2*p - scrollbar_width - 40;
    
    for (int i = scroll_offset; i < cnt && (i - scroll_offset) < max_display; i++) {
        ProcessInfo *proc = &sorted[i];
        
        unsigned long rc = (i - scroll_offset) % 2 == 0 ? COLOR_BG_CARD : 0x11161D;
        draw_rect(p+16, yp-22, rw, row_height, rc);
        
        unsigned long tc = COLOR_TEXT;
        if (proc->pid == selected_pid) {
            draw_rect(p+16, yp-22, rw, row_height, 0x1A3A5C);
            tc = COLOR_TEXT_BRIGHT;
        }
        
        char buf[64];
        snprintf(buf, sizeof(buf), "%d", proc->pid);
        draw_text(p+24, yp, buf, tc);
        
        char user_name[25];
        strncpy(user_name, proc->user, 24);
        user_name[24] = '\0';
        draw_text(p+140, yp, user_name, COLOR_TEXT_SEC);
        
        char name[25];
        strncpy(name, proc->name, 24);
        name[24] = '\0';
        draw_text(p+260, yp, name, tc);
        
        int cpu = proc->cpu_usage;
        if (cpu > 100) cpu = 100;
        draw_progress(p+450, yp-16, 90, 24, cpu, COLOR_ACCENT);
        snprintf(buf, sizeof(buf), "%3d%%", cpu);
        draw_text(p+548, yp, buf, COLOR_ACCENT);
        
        long mem = proc->memory_usage / 1024;
        unsigned long mc = COLOR_ACCENT_GREEN;
        if (mem > 1000) mc = COLOR_ACCENT_ORANGE;
        if (mem > 5000) mc = COLOR_ACCENT_RED;
        snprintf(buf, sizeof(buf), "%5ld", mem);
        draw_text(p+580, yp, buf, mc);
        
        char st[5];
        unsigned long sc = COLOR_TEXT_SEC;
        switch(proc->state) {
            case 'R': strcpy(st, "RUN"); sc = COLOR_ACCENT_GREEN; break;
            case 'S': strcpy(st, "SLP"); sc = COLOR_ACCENT; break;
            case 'Z': strcpy(st, "ZMB"); sc = COLOR_ACCENT_ORANGE; break;
            case 'T': strcpy(st, "STP"); sc = COLOR_ACCENT_RED; break;
            default: snprintf(st, 5, "%c", proc->state); break;
        }
        draw_text(p+700, yp, st, sc);
        
        char cmd[32];
        strncpy(cmd, proc->cmdline, 30);
        cmd[30] = '\0';
        draw_text(p+820, yp, cmd, COLOR_TEXT_SEC);
        
        yp += row_height;
    }
    
    int fy = win_height - fh;
    draw_rect(0, fy, win_width, fh, COLOR_BG_CARD);
    draw_rect(0, fy, win_width, 1, COLOR_BORDER);
    
    char footer[256];
    int disp = scroll_offset + max_display;
    if (disp > cnt) disp = cnt;
    snprintf(footer, sizeof(footer),
             "Total: %d  |  Showing: %d-%d  |  Sorted: MEMORY  |  Q:Quit  Up/Down:Scroll  Click:Select",
             cnt, scroll_offset + 1, disp);
    draw_text(p+16, fy+38, footer, COLOR_TEXT_SEC);
    
    if (settings.show_settings) {
        draw_settings_window();
    }
    
    XFlush(dpy);
}

static void* collect_thread(void *arg) {
    (void)arg;
    while (running) {
        read_process_data();
        if (settings.sleep_mode) {
            usleep(500000);
        } else {
            usleep(100000);
        }
    }
    return NULL;
}

static void sig_handler(int sig) {
    (void)sig;
    running = false;
}

static void handle_settings_click(int x, int y) {
    int sw = 480, sh = 300;
    int sx = (win_width - sw) / 2;
    int sy = (win_height - sh) / 2;
    
    if (x >= sx + 100 && x <= sx + sw - 100 &&
        y >= sy + sh - 66 && y <= sy + sh - 18) {
        settings.show_settings = 0;
        return;
    }
    
    int ypos = sy + 96;
    int ctrl_x = sx + 200;
    int ctrl_w = 240;
    int row_h = 50;
    
    if (x >= ctrl_x && x <= ctrl_x + ctrl_w &&
        y >= ypos && y <= ypos + row_h) {
        settings.sleep_mode = !settings.sleep_mode;
        return;
    }
    ypos += row_h + 14;
    
    if (x >= ctrl_x && x <= ctrl_x + ctrl_w &&
        y >= ypos && y <= ypos + row_h) {
        settings.dark_mode = !settings.dark_mode;
        update_theme();
        return;
    }
}

/* ============================================================
   MAIN
   ============================================================ */
int main(int argc, char **argv) {
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = true;
    }
    
    printf("\n");
    printf("============================================================\n");
    printf("  XRAY-SCOPE v1.0 - System Monitor\n");
    printf("============================================================\n\n");
    printf("Mode: %s\n", headless ? "HEADLESS" : "GUI");
    printf("Controls: Up/Down scroll, click to select, Settings button\n");
    printf("Processes sorted by: MEMORY (highest usage first)\n");
    printf("Font size: 40px (Very Large)\n\n");
    
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    memset(prev_cpu, 0, sizeof(prev_cpu));
    prev_system_total = read_system_cpu_total();
    first_run = 1;
    
    update_theme();
    
    pthread_t thread;
    if (pthread_create(&thread, NULL, collect_thread, NULL) != 0) {
        fprintf(stderr, "Thread failed\n");
        return 1;
    }
    
    if (headless) {
        while (running) {
            sleep(2);
            printf("\rProcesses: %d   ", proc_data.count);
            fflush(stdout);
        }
    } else {
        dpy = XOpenDisplay(NULL);
        if (!dpy) {
            fprintf(stderr, "Cannot open display\n");
            return 1;
        }
        
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
        
        XSetWindowAttributes attrs;
        attrs.background_pixel = COLOR_BG;
        attrs.border_pixel = COLOR_BORDER;
        attrs.event_mask = KeyPressMask | ExposureMask | StructureNotifyMask | 
                           ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
        
        win = XCreateWindow(dpy, root, 0, 0, win_width, win_height, 2,
                            CopyFromParent, InputOutput, CopyFromParent,
                            CWBackPixel | CWBorderPixel | CWEventMask, &attrs);
        
        XStoreName(dpy, win, "XRAY-SCOPE");
        XMapWindow(dpy, win);
        
        gc = XCreateGC(dpy, win, 0, NULL);
        load_fonts();
        
        XEvent ev;
        int timeout = 200;
        while (timeout-- > 0) {
            if (XCheckTypedWindowEvent(dpy, win, MapNotify, &ev)) {
                break;
            }
            usleep(10000);
        }
        
        XSizeHints hints;
        hints.flags = PMinSize;
        hints.min_width = 1000;
        hints.min_height = 650;
        XSetWMNormalHints(dpy, win, &hints);
        
        printf("Running... (Press Ctrl+C or 'q' to stop)\n\n");
        
        while (running) {
            while (XPending(dpy) > 0) {
                XNextEvent(dpy, &ev);
                
                if (ev.type == KeyPress) {
                    KeySym key = XLookupKeysym(&ev.xkey, 0);
                    if (key == XK_q || key == XK_Q || key == XK_Escape) {
                        running = false;
                        break;
                    }
                    if (key == XK_Up) {
                        if (scroll_offset > 0) scroll_offset--;
                    }
                    if (key == XK_Down) {
                        int max_scroll = proc_data.count - 20;
                        if (max_scroll < 0) max_scroll = 0;
                        if (scroll_offset < max_scroll) scroll_offset++;
                    }
                }
                
                if (ev.type == ConfigureNotify) {
                    win_width = ev.xconfigure.width;
                    win_height = ev.xconfigure.height;
                }
                
                if (ev.type == MotionNotify) {
                    int x = ev.xmotion.x;
                    int y = ev.xmotion.y;
                    settings_hover = (x >= settings_btn_x && x <= settings_btn_x + settings_btn_w &&
                                      y >= settings_btn_y && y <= settings_btn_y + settings_btn_h);
                }
                
                if (ev.type == ButtonPress) {
                    int x = ev.xbutton.x;
                    int y = ev.xbutton.y;
                    
                    if (x >= settings_btn_x && x <= settings_btn_x + settings_btn_w &&
                        y >= settings_btn_y && y <= settings_btn_y + settings_btn_h) {
                        settings.show_settings = !settings.show_settings;
                        continue;
                    }
                    
                    if (settings.show_settings) {
                        int sw = 480, sh = 300;
                        int sx = (win_width - sw) / 2;
                        int sy = (win_height - sh) / 2;
                        if (x >= sx && x <= sx + sw && y >= sy && y <= sy + sh) {
                            handle_settings_click(x, y);
                            continue;
                        } else {
                            settings.show_settings = 0;
                            continue;
                        }
                    }
                    
                    int sb_x = win_width - scrollbar_width - 15;
                    if (x >= sb_x && x <= sb_x + scrollbar_width) {
                        scrollbar_dragging = 1;
                        continue;
                    }
                    
                    int ty = 80 + 66 + 34 + 42;
                    int idx = (y - ty) / row_height + scroll_offset;
                    if (idx >= 0 && idx < proc_data.count) {
                        ProcessInfo sorted[MAX_PROCESSES];
                        sort_processes(sorted, proc_data.count);
                        if (idx < proc_data.count) {
                            selected_pid = sorted[idx].pid;
                        }
                    }
                }
                
                if (ev.type == ButtonRelease) {
                    scrollbar_dragging = 0;
                }
                
                if (ev.type == MotionNotify && scrollbar_dragging) {
                    int sb_y = 180;
                    int sb_h = win_height - 260;
                    int total_rows = proc_data.count;
                    int visible_rows = 20;
                    int max_scroll = total_rows - visible_rows;
                    if (max_scroll < 0) max_scroll = 0;
                    
                    int thumb_h = sb_h * visible_rows / total_rows;
                    if (thumb_h < 40) thumb_h = 40;
                    
                    int new_offset = (ev.xmotion.y - sb_y - thumb_h/2) * max_scroll / (sb_h - thumb_h);
                    if (new_offset < 0) new_offset = 0;
                    if (new_offset > max_scroll) new_offset = max_scroll;
                    scroll_offset = new_offset;
                }
            }
            
            render_gui();
            usleep(30000);
        }
        
        if (font && font != font_bold) XFreeFont(dpy, font);
        if (font_bold && font_bold != font) XFreeFont(dpy, font_bold);
        if (gc) XFreeGC(dpy, gc);
        if (win) XDestroyWindow(dpy, win);
        if (dpy) XCloseDisplay(dpy);
    }
    
    running = false;
    pthread_join(thread, NULL);
    
    printf("\nXRAY-SCOPE stopped successfully\n");
    return 0;
}

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
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <Imlib2.h>

#define VERSION "1.0"
#define MAX_PROCESSES 4096
#define MAX_PATH 1024
#define RADIUS 10

#define COLOR_ACCENT           0x58A6FF
#define COLOR_ACCENT_GREEN     0x3FB950
#define COLOR_ACCENT_ORANGE    0xF0883E
#define COLOR_ACCENT_RED       0xF85149

#define COLOR_BG_DARK          0x0D1117
#define COLOR_BG_CARD_DARK     0x161B22
#define COLOR_BG_HEADER_DARK   0x0A0E14
#define COLOR_TEXT_DARK        0xE6EDF3
#define COLOR_TEXT_SEC_DARK    0x8B949E
#define COLOR_TEXT_BRIGHT_DARK 0xFFFFFF
#define COLOR_FOLDER_DARK      0xF0883E
#define COLOR_FILE_DARK        0x8B949E
#define COLOR_BORDER_DARK      0x30363D
#define COLOR_SELECTED_DARK    0x1A3A5C
#define COLOR_SCROLL_BG_DARK   0x1C2333
#define COLOR_SCROLL_THUMB_DARK 0x58A6FF
#define COLOR_PREVIEW_BG_DARK  0x0A0E14
#define COLOR_LINE_NUM_DARK    0x2D3748
#define COLOR_SETTINGS_BG_DARK 0x1A2332

#define COLOR_BG_LIGHT          0xFFFFFF
#define COLOR_BG_CARD_LIGHT     0xF0F0F0
#define COLOR_BG_HEADER_LIGHT   0xE8E8E8
#define COLOR_TEXT_LIGHT        0x1A1A1A
#define COLOR_TEXT_SEC_LIGHT    0x555555
#define COLOR_TEXT_BRIGHT_LIGHT 0x000000
#define COLOR_FOLDER_LIGHT      0xCC6600
#define COLOR_FILE_LIGHT        0x333333
#define COLOR_BORDER_LIGHT      0xCCCCCC
#define COLOR_SELECTED_LIGHT    0xB3D4FC
#define COLOR_SCROLL_BG_LIGHT   0xDDDDDD
#define COLOR_SCROLL_THUMB_LIGHT 0x666666
#define COLOR_PREVIEW_BG_LIGHT  0xF5F5F5
#define COLOR_LINE_NUM_LIGHT    0xCCCCCC
#define COLOR_SETTINGS_BG_LIGHT 0xE8E8E8

static unsigned long COLOR_BG = COLOR_BG_DARK;
static unsigned long COLOR_BG_CARD = COLOR_BG_CARD_DARK;
static unsigned long COLOR_BG_HEADER = COLOR_BG_HEADER_DARK;
static unsigned long COLOR_TEXT = COLOR_TEXT_DARK;
static unsigned long COLOR_TEXT_SEC = COLOR_TEXT_SEC_DARK;
static unsigned long COLOR_TEXT_BRIGHT = COLOR_TEXT_BRIGHT_DARK;
static unsigned long COLOR_FOLDER = COLOR_FOLDER_DARK;
static unsigned long COLOR_FILE = COLOR_FILE_DARK;
static unsigned long COLOR_BORDER = COLOR_BORDER_DARK;
static unsigned long COLOR_SELECTED = COLOR_SELECTED_DARK;
static unsigned long COLOR_SCROLL_BG = COLOR_SCROLL_BG_DARK;
static unsigned long COLOR_SCROLL_THUMB = COLOR_SCROLL_THUMB_DARK;
static unsigned long COLOR_PREVIEW_BG = COLOR_PREVIEW_BG_DARK;
static unsigned long COLOR_LINE_NUM = COLOR_LINE_NUM_DARK;
static unsigned long COLOR_SETTINGS_BG = COLOR_SETTINGS_BG_DARK;

typedef struct {
    int sleep_mode;
    int show_settings;
    int dark_mode;
    int file_mode;
    char current_path[MAX_PATH];
    int file_scroll_offset;
    int selected_file_index;
    int show_preview;
    int tree_width;
    int auto_open_video;
} Settings;

static Settings settings = {
    .sleep_mode = 0, .show_settings = 0, .dark_mode = 1,
    .file_mode = 0, .current_path = "/", .file_scroll_offset = 0,
    .selected_file_index = 0, .show_preview = 1, .tree_width = 380,
    .auto_open_video = 0
};

static void update_theme_colors(void) {
    if (settings.dark_mode) {
        COLOR_BG = COLOR_BG_DARK; COLOR_BG_CARD = COLOR_BG_CARD_DARK;
        COLOR_BG_HEADER = COLOR_BG_HEADER_DARK; COLOR_TEXT = COLOR_TEXT_DARK;
        COLOR_TEXT_SEC = COLOR_TEXT_SEC_DARK; COLOR_TEXT_BRIGHT = COLOR_TEXT_BRIGHT_DARK;
        COLOR_FOLDER = COLOR_FOLDER_DARK; COLOR_FILE = COLOR_FILE_DARK;
        COLOR_BORDER = COLOR_BORDER_DARK; COLOR_SELECTED = COLOR_SELECTED_DARK;
        COLOR_SCROLL_BG = COLOR_SCROLL_BG_DARK; COLOR_SCROLL_THUMB = COLOR_SCROLL_THUMB_DARK;
        COLOR_PREVIEW_BG = COLOR_PREVIEW_BG_DARK; COLOR_LINE_NUM = COLOR_LINE_NUM_DARK;
        COLOR_SETTINGS_BG = COLOR_SETTINGS_BG_DARK;
    } else {
        COLOR_BG = COLOR_BG_LIGHT; COLOR_BG_CARD = COLOR_BG_CARD_LIGHT;
        COLOR_BG_HEADER = COLOR_BG_HEADER_LIGHT; COLOR_TEXT = COLOR_TEXT_LIGHT;
        COLOR_TEXT_SEC = COLOR_TEXT_SEC_LIGHT; COLOR_TEXT_BRIGHT = COLOR_TEXT_BRIGHT_LIGHT;
        COLOR_FOLDER = COLOR_FOLDER_LIGHT; COLOR_FILE = COLOR_FILE_LIGHT;
        COLOR_BORDER = COLOR_BORDER_LIGHT; COLOR_SELECTED = COLOR_SELECTED_LIGHT;
        COLOR_SCROLL_BG = COLOR_SCROLL_BG_LIGHT; COLOR_SCROLL_THUMB = COLOR_SCROLL_THUMB_LIGHT;
        COLOR_PREVIEW_BG = COLOR_PREVIEW_BG_LIGHT; COLOR_LINE_NUM = COLOR_LINE_NUM_LIGHT;
        COLOR_SETTINGS_BG = COLOR_SETTINGS_BG_LIGHT;
    }
}

typedef struct FileNode {
    char name[256];
    char path[MAX_PATH];
    int is_dir;
    int is_image;
    int is_video;
    int is_text;
    int depth;
    off_t size;
    time_t mtime;
    struct FileNode *children;
    struct FileNode *next;
    int child_count;
    int expanded;
} FileNode;

typedef struct { FileNode *root; int count; } FileTree;

static FileTree file_tree;
static char preview_content[1024*32];
static int preview_size = 0;
static int preview_type = 0;
static FileNode **flat_list = NULL;
static int flat_count = 0;

static Imlib_Image current_image = NULL;
static char current_image_path[MAX_PATH] = "";
static int current_image_w = 0;
static int current_image_h = 0;
static Window video_window = 0;
static int video_x = 0, video_y = 0, video_w = 0, video_h = 0;
static pid_t video_pid = 0;
static char current_video_path[MAX_PATH] = "";

typedef struct {
    char name[64]; int pid; int ppid;
    char cmdline[512]; char user[64]; char state;
    long cpu_usage; long memory_usage; long num_threads;
} ProcessInfo;

typedef struct {
    ProcessInfo processes[MAX_PROCESSES];
    int count; time_t timestamp;
} ProcessData;

static ProcessData proc_data;
static int scroll_offset = 0;
static int selected_pid = -1;
static volatile bool running = true;
static int scrollbar_width = 12;
static int scrollbar_dragging = 0;
static int settings_hover = 0;
static int files_btn_hover = 0;
static int process_btn_hover = 0;

static Display *dpy;
static Window win;
static Pixmap back_buffer = 0;
static GC gc;
static int win_width = 1300;
static int win_height = 850;
static XFontStruct *font;
static XFontStruct *font_bold;
static int font_height = 20;
static int row_height = 26;
static int settings_btn_x = 0, settings_btn_y = 0, settings_btn_w = 100, settings_btn_h = 34;
static int files_btn_x = 0, files_btn_y = 0, files_btn_w = 80, files_btn_h = 34;

static void create_back_buffer(void) {
    if (back_buffer) XFreePixmap(dpy, back_buffer);
    back_buffer = XCreatePixmap(dpy, win, win_width, win_height,
                                 DefaultDepth(dpy, DefaultScreen(dpy)));
}

static void flush_back_buffer(void) {
    XCopyArea(dpy, back_buffer, win, gc, 0, 0, win_width, win_height, 0, 0);
    XFlush(dpy);
}

typedef struct { unsigned long long total_time; } CpuData;
static CpuData prev_cpu[MAX_PROCESSES];
static unsigned long long prev_system_total = 0;
static int first_run = 1;

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
                    if (field == 2) { p->state = token[0]; break; }
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
            if (fscanf(f, "%*ld %ld", &resident) == 1) p->memory_usage = resident * 4;
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
                    if (pw) strncpy(p->user, pw->pw_name, sizeof(p->user)-1);
                    else snprintf(p->user, sizeof(p->user), "%u", uid);
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
            }
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

static int is_image_file(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".gif") == 0 ||
            strcasecmp(ext, ".bmp") == 0 || strcasecmp(ext, ".svg") == 0 ||
            strcasecmp(ext, ".webp") == 0 || strcasecmp(ext, ".ico") == 0 ||
            strcasecmp(ext, ".tiff") == 0 || strcasecmp(ext, ".tif") == 0);
}

static int is_video_file(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return (strcasecmp(ext, ".mp4") == 0 || strcasecmp(ext, ".avi") == 0 ||
            strcasecmp(ext, ".mkv") == 0 || strcasecmp(ext, ".mov") == 0 ||
            strcasecmp(ext, ".wmv") == 0 || strcasecmp(ext, ".flv") == 0 ||
            strcasecmp(ext, ".webm") == 0 || strcasecmp(ext, ".m4v") == 0 ||
            strcasecmp(ext, ".mpg") == 0 || strcasecmp(ext, ".mpeg") == 0 ||
            strcasecmp(ext, ".3gp") == 0 || strcasecmp(ext, ".ogv") == 0);
}

static int is_text_file(const char *name) {
    const char *ext = strrchr(name, '.');
    const char *basename = strrchr(name, '/');
    if (basename) basename++; else basename = name;

    if (strcasecmp(basename, "Makefile") == 0 || strcasecmp(basename, "Dockerfile") == 0 ||
        strcasecmp(basename, "Jenkinsfile") == 0 || strcasecmp(basename, "Vagrantfile") == 0 ||
        strcasecmp(basename, "Gemfile") == 0 || strcasecmp(basename, "Rakefile") == 0 ||
        strcasecmp(basename, "CMakeLists.txt") == 0 || strcasecmp(basename, ".gitignore") == 0 ||
        strcasecmp(basename, ".env") == 0 || strcasecmp(basename, ".bashrc") == 0 ||
        strcasecmp(basename, ".zshrc") == 0 || strcasecmp(basename, ".profile") == 0 ||
        strcasecmp(basename, "LICENSE") == 0 || strcasecmp(basename, "README") == 0 ||
        strcasecmp(basename, "CHANGELOG") == 0) return 1;

    if (!ext) return 0;

    if (strcasecmp(ext, ".c") == 0 || strcasecmp(ext, ".h") == 0 ||
        strcasecmp(ext, ".cpp") == 0 || strcasecmp(ext, ".cc") == 0 ||
        strcasecmp(ext, ".cxx") == 0 || strcasecmp(ext, ".hpp") == 0 ||
        strcasecmp(ext, ".hxx") == 0 || strcasecmp(ext, ".hh") == 0) return 1;
    if (strcasecmp(ext, ".java") == 0 || strcasecmp(ext, ".kt") == 0 ||
        strcasecmp(ext, ".kts") == 0 || strcasecmp(ext, ".scala") == 0 ||
        strcasecmp(ext, ".groovy") == 0 || strcasecmp(ext, ".gradle") == 0) return 1;
    if (strcasecmp(ext, ".py") == 0 || strcasecmp(ext, ".pyw") == 0 ||
        strcasecmp(ext, ".pyx") == 0 || strcasecmp(ext, ".rb") == 0 ||
        strcasecmp(ext, ".erb") == 0 || strcasecmp(ext, ".pl") == 0 ||
        strcasecmp(ext, ".pm") == 0) return 1;
    if (strcasecmp(ext, ".js") == 0 || strcasecmp(ext, ".jsx") == 0 ||
        strcasecmp(ext, ".ts") == 0 || strcasecmp(ext, ".tsx") == 0 ||
        strcasecmp(ext, ".mjs") == 0 || strcasecmp(ext, ".cjs") == 0) return 1;
    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0 ||
        strcasecmp(ext, ".css") == 0 || strcasecmp(ext, ".scss") == 0 ||
        strcasecmp(ext, ".sass") == 0 || strcasecmp(ext, ".less") == 0) return 1;
    if (strcasecmp(ext, ".json") == 0 || strcasecmp(ext, ".xml") == 0 ||
        strcasecmp(ext, ".yaml") == 0 || strcasecmp(ext, ".yml") == 0 ||
        strcasecmp(ext, ".toml") == 0 || strcasecmp(ext, ".ini") == 0 ||
        strcasecmp(ext, ".cfg") == 0 || strcasecmp(ext, ".conf") == 0 ||
        strcasecmp(ext, ".properties") == 0) return 1;
    if (strcasecmp(ext, ".sh") == 0 || strcasecmp(ext, ".bash") == 0 ||
        strcasecmp(ext, ".zsh") == 0 || strcasecmp(ext, ".fish") == 0 ||
        strcasecmp(ext, ".bat") == 0 || strcasecmp(ext, ".cmd") == 0 ||
        strcasecmp(ext, ".ps1") == 0) return 1;
    if (strcasecmp(ext, ".md") == 0 || strcasecmp(ext, ".markdown") == 0 ||
        strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".rst") == 0 ||
        strcasecmp(ext, ".adoc") == 0 || strcasecmp(ext, ".tex") == 0 ||
        strcasecmp(ext, ".latex") == 0) return 1;
    if (strcasecmp(ext, ".go") == 0 || strcasecmp(ext, ".rs") == 0 ||
        strcasecmp(ext, ".swift") == 0 || strcasecmp(ext, ".dart") == 0) return 1;
    if (strcasecmp(ext, ".cs") == 0 || strcasecmp(ext, ".fs") == 0 ||
        strcasecmp(ext, ".fsx") == 0 || strcasecmp(ext, ".vb") == 0) return 1;
    if (strcasecmp(ext, ".php") == 0 || strcasecmp(ext, ".phtml") == 0) return 1;
    if (strcasecmp(ext, ".sql") == 0) return 1;
    if (strcasecmp(ext, ".cmake") == 0 || strcasecmp(ext, ".mk") == 0 ||
        strcasecmp(ext, ".make") == 0) return 1;
    if (strcasecmp(ext, ".log") == 0) return 1;
    if (strcasecmp(ext, ".diff") == 0 || strcasecmp(ext, ".patch") == 0) return 1;
    if (strcasecmp(ext, ".lua") == 0 || strcasecmp(ext, ".r") == 0 ||
        strcasecmp(ext, ".R") == 0) return 1;
    if (strcasecmp(ext, ".hs") == 0 || strcasecmp(ext, ".ex") == 0 ||
        strcasecmp(ext, ".exs") == 0 || strcasecmp(ext, ".erl") == 0 ||
        strcasecmp(ext, ".hrl") == 0) return 1;
    if (strcasecmp(ext, ".m") == 0 || strcasecmp(ext, ".mm") == 0) return 1;
    if (strcasecmp(ext, ".asm") == 0 || strcasecmp(ext, ".s") == 0) return 1;
    return 0;
}

static void load_file_content(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { preview_type = 0; preview_size = 0; return; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 1024*32) size = 1024*32;
    preview_size = fread(preview_content, 1, size, f);
    preview_content[preview_size] = '\0';
    fclose(f);
    preview_type = 3;
}

static void clear_image_cache(void) {
    if (current_image) {
        imlib_context_set_image(current_image);
        imlib_free_image();
        current_image = NULL;
    }
    current_image_path[0] = '\0';
    current_image_w = 0;
    current_image_h = 0;
}

static void load_image_cached(const char *path) {
    if (strcmp(current_image_path, path) == 0 && current_image != NULL) return;
    clear_image_cache();
    current_image = imlib_load_image(path);
    if (!current_image) { current_image_path[0] = '\0'; return; }
    imlib_context_set_image(current_image);
    current_image_w = imlib_image_get_width();
    current_image_h = imlib_image_get_height();
    strncpy(current_image_path, path, sizeof(current_image_path)-1);
    current_image_path[sizeof(current_image_path)-1] = '\0';
}

static void render_cached_image(int x, int y, int w, int h) {
    if (!current_image) return;
    imlib_context_set_image(current_image);
    imlib_context_set_drawable(back_buffer);
    imlib_context_set_anti_alias(1);
    imlib_context_set_blend(0);
    imlib_context_set_color_modifier(NULL);

    double scale_x = (double)w / current_image_w;
    double scale_y = (double)h / current_image_h;
    double scale = (scale_x < scale_y) ? scale_x : scale_y;

    int new_w = current_image_w * scale;
    int new_h = current_image_h * scale;
    int offset_x = x + (w - new_w) / 2;
    int offset_y = y + (h - new_h) / 2;

    imlib_render_image_part_on_drawable_at_size(
        0, 0, current_image_w, current_image_h,
        offset_x, offset_y, new_w, new_h
    );
}

/* ============================================================
   VIDEO DISPLAY - FIXED: --wid=VALUE + NO close(ConnectionNumber)
   ============================================================ */
static void stop_video(void) {
    if (video_pid > 0) {
        kill(video_pid, SIGTERM);
        waitpid(video_pid, NULL, WNOHANG);
        video_pid = 0;
    }
    if (video_window) {
        XDestroyWindow(dpy, video_window);
        XFlush(dpy);
        video_window = 0;
    }
    current_video_path[0] = '\0';
}

static void start_video(const char *path, int x, int y, int w, int h) {
    if (strcmp(current_video_path, path) == 0 && video_window != 0 && video_pid > 0) {
        if (kill(video_pid, 0) == 0) return;
    }

    stop_video();

    video_x = x; video_y = y; video_w = w; video_h = h;

    XSetWindowAttributes vattrs;
    vattrs.background_pixel = 0x000000;
    vattrs.border_pixel = 0x000000;
    vattrs.override_redirect = False;
    vattrs.event_mask = 0;

    video_window = XCreateWindow(dpy, win, x, y, w, h, 0,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWBackPixel | CWBorderPixel, &vattrs);
    XMapWindow(dpy, video_window);
    XRaiseWindow(dpy, video_window);

    /* CRITICAL: XSync ensures window exists on X server */
    XSync(dpy, False);
    usleep(100000);

    /* CRITICAL: Use --wid=VALUE format (with = sign) for mpv */
    char wid_arg[64];
    snprintf(wid_arg, sizeof(wid_arg), "--wid=%lu", (unsigned long)video_window);

    pid_t pid = fork();
    if (pid == 0) {
        /* Child process */
        setsid();
        
        /* CRITICAL: DO NOT close(ConnectionNumber(dpy)) - it breaks X connection */
        /* Just redirect output and exec mpv */
        
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        /* Execute mpv with --wid=VALUE format */
        execlp("mpv", "mpv",
               wid_arg,
               "--force-window=immediate",
               "--no-border",
               "--no-osc",
               "--no-osd-bar",
               "--no-input-default-bindings",
               "--no-keepaspect-window",
               "--really-quiet",
               path,
               NULL);
        _exit(1);
    } else if (pid > 0) {
        video_pid = pid;
        strncpy(current_video_path, path, sizeof(current_video_path)-1);
        current_video_path[sizeof(current_video_path)-1] = '\0';
    }
}

static void raise_video_window(void) {
    if (video_window) {
        XMoveResizeWindow(dpy, video_window, video_x, video_y, video_w, video_h);
        XRaiseWindow(dpy, video_window);
    }
}

static FileNode* build_tree(const char *path, int depth) {
    DIR *dir = opendir(path);
    if (!dir) return NULL;

    FileNode *root = malloc(sizeof(FileNode));
    if (!root) return NULL;

    const char *last = strrchr(path, '/');
    if (last && strlen(last) > 1) strncpy(root->name, last + 1, sizeof(root->name)-1);
    else strncpy(root->name, path, sizeof(root->name)-1);
    root->name[sizeof(root->name)-1] = '\0';
    strncpy(root->path, path, sizeof(root->path)-1);
    root->path[sizeof(root->path)-1] = '\0';
    root->is_dir = 1;
    root->is_image = 0; root->is_video = 0; root->is_text = 0;
    root->depth = depth; root->child_count = 0; root->expanded = 1;
    root->children = NULL; root->next = NULL;
    root->size = 0; root->mtime = 0;

    struct dirent *entry;
    struct stat st;
    char full_path[MAX_PATH];
    FileNode *prev = NULL;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "..") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &st) != 0) continue;

        FileNode *node = malloc(sizeof(FileNode));
        if (!node) continue;

        strncpy(node->name, entry->d_name, sizeof(node->name)-1);
        node->name[sizeof(node->name)-1] = '\0';
        strncpy(node->path, full_path, sizeof(node->path)-1);
        node->path[sizeof(node->path)-1] = '\0';
        node->is_dir = S_ISDIR(st.st_mode);
        node->is_image = is_image_file(entry->d_name);
        node->is_video = is_video_file(entry->d_name);
        node->is_text = is_text_file(entry->d_name);
        node->depth = depth + 1;
        node->size = st.st_size;
        node->mtime = st.st_mtime;
        node->child_count = 0; node->expanded = 0;
        node->children = NULL; node->next = NULL;

        if (node->is_dir) {
            node->children = build_tree(full_path, depth + 1);
            if (node->children) node->expanded = 0;
        }

        if (prev) prev->next = node;
        else root->children = node;
        prev = node;
        root->child_count++;
    }
    closedir(dir);
    return root;
}

static void flatten_tree(FileNode *node, FileNode **list, int *count, int max_count) {
    if (!node || *count >= max_count) return;
    list[*count] = node; (*count)++;
    if (node->expanded && node->children)
        flatten_tree(node->children, list, count, max_count);
    if (node->next)
        flatten_tree(node->next, list, count, max_count);
}

static void free_tree(FileNode *node) {
    if (!node) return;
    if (node->children) free_tree(node->children);
    if (node->next) free_tree(node->next);
    free(node);
}

static void update_tree_flat(void) {
    flat_count = 0;
    if (flat_list) free(flat_list);
    flat_list = malloc(sizeof(FileNode*) * 4096);
    if (!flat_list) return;
    if (file_tree.root)
        flatten_tree(file_tree.root, flat_list, &flat_count, 4096);
    if (settings.selected_file_index >= flat_count)
        settings.selected_file_index = flat_count - 1;
    if (settings.selected_file_index < 0) settings.selected_file_index = 0;
}

static void load_fonts(void) {
    font = XLoadQueryFont(dpy, "-misc-dejavu sans mono-medium-r-normal--20-*-*-*-*-*-*-*");
    if (!font) font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--18-*-*-*-*-*-*-*");
    if (!font) font = XLoadQueryFont(dpy, "fixed");
    if (font) {
        font_height = font->ascent + font->descent;
        row_height = font_height + 6;
        XSetFont(dpy, gc, font->fid);
    }
    font_bold = XLoadQueryFont(dpy, "-misc-dejavu sans mono-bold-r-normal--20-*-*-*-*-*-*-*");
    if (!font_bold) font_bold = font;
}

static void draw_rounded_rect(int x, int y, int w, int h, unsigned long color) {
    int r = RADIUS;
    if (w < 2*r || h < 2*r) {
        XSetForeground(dpy, gc, color);
        XFillRectangle(dpy, back_buffer, gc, x, y, w, h);
        return;
    }
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, back_buffer, gc, x + r, y, w - 2*r, h);
    XFillRectangle(dpy, back_buffer, gc, x, y + r, w, h - 2*r);
    XFillArc(dpy, back_buffer, gc, x, y, 2*r, 2*r, 0, 360*64);
    XFillArc(dpy, back_buffer, gc, x + w - 2*r, y, 2*r, 2*r, 0, 360*64);
    XFillArc(dpy, back_buffer, gc, x, y + h - 2*r, 2*r, 2*r, 0, 360*64);
    XFillArc(dpy, back_buffer, gc, x + w - 2*r, y + h - 2*r, 2*r, 2*r, 0, 360*64);
}

static void draw_rounded_rect_border(int x, int y, int w, int h, unsigned long color) {
    int r = RADIUS;
    if (w < 2*r || h < 2*r) {
        XSetForeground(dpy, gc, color);
        XDrawRectangle(dpy, back_buffer, gc, x, y, w, h);
        return;
    }
    XSetForeground(dpy, gc, color);
    XDrawLine(dpy, back_buffer, gc, x + r, y, x + w - r, y);
    XDrawLine(dpy, back_buffer, gc, x + r, y + h, x + w - r, y + h);
    XDrawLine(dpy, back_buffer, gc, x, y + r, x, y + h - r);
    XDrawLine(dpy, back_buffer, gc, x + w, y + r, x + w, y + h - r);
    XDrawArc(dpy, back_buffer, gc, x, y, 2*r, 2*r, 0, 360*64);
    XDrawArc(dpy, back_buffer, gc, x + w - 2*r, y, 2*r, 2*r, 0, 360*64);
    XDrawArc(dpy, back_buffer, gc, x, y + h - 2*r, 2*r, 2*r, 0, 360*64);
    XDrawArc(dpy, back_buffer, gc, x + w - 2*r, y + h - 2*r, 2*r, 2*r, 0, 360*64);
}

static void draw_rect(int x, int y, int w, int h, unsigned long color) {
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, back_buffer, gc, x, y, w, h);
}

static void draw_text(int x, int y, const char *text, unsigned long color) {
    XSetForeground(dpy, gc, color);
    XDrawString(dpy, back_buffer, gc, x, y, text, strlen(text));
}

static void draw_text_bold(int x, int y, const char *text, unsigned long color) {
    if (font_bold && font_bold != font) XSetFont(dpy, gc, font_bold->fid);
    XSetForeground(dpy, gc, color);
    XDrawString(dpy, back_buffer, gc, x, y, text, strlen(text));
    if (font) XSetFont(dpy, gc, font->fid);
}

static void draw_scrollbar(int total_rows, int visible_rows, int offset, int x, int y, int h) {
    if (total_rows <= visible_rows) return;
    draw_rect(x, y, scrollbar_width, h, COLOR_SCROLL_BG);
    float ratio = (float)visible_rows / total_rows;
    int thumb_h = h * ratio;
    if (thumb_h < 20) thumb_h = 20;
    int max_scroll = total_rows - visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    float scroll_ratio = (max_scroll > 0) ? (float)offset / max_scroll : 0;
    int thumb_y = y + (h - thumb_h) * scroll_ratio;
    draw_rect(x + 1, thumb_y, scrollbar_width - 2, thumb_h, COLOR_SCROLL_THUMB);
}

static void draw_settings_window(void) {
    int sw = 380, sh = 280;
    int sx = (win_width - sw) / 2;
    int sy = (win_height - sh) / 2;

    draw_rounded_rect(sx, sy, sw, sh, COLOR_SETTINGS_BG);
    draw_rounded_rect_border(sx, sy, sw, sh, COLOR_BORDER);
    draw_rect(sx+1, sy+1, sw-2, sh-2, COLOR_SETTINGS_BG);

    draw_text_bold(sx+16, sy+36, "Settings", COLOR_ACCENT);
    draw_rect(sx+10, sy+48, sw-20, 1, COLOR_BORDER);

    int ypos = sy + 68;
    int label_x = sx + 16;
    int ctrl_x = sx + 150;
    int ctrl_w = 190;
    int row_h = 32;

    draw_text(label_x, ypos+12, "Sleep:", COLOR_TEXT);
    unsigned long sleep_col = settings.sleep_mode ? 0x1A3A5C : 0x2D3748;
    draw_rounded_rect(ctrl_x, ypos, ctrl_w, row_h, sleep_col);
    draw_rounded_rect_border(ctrl_x, ypos, ctrl_w, row_h, COLOR_BORDER);
    draw_text(ctrl_x+8, ypos+22, settings.sleep_mode ? "ON" : "OFF",
              settings.sleep_mode ? COLOR_ACCENT_GREEN : COLOR_TEXT_SEC);
    ypos += row_h + 8;

    draw_text(label_x, ypos+12, "Theme:", COLOR_TEXT);
    unsigned long theme_col = settings.dark_mode ? 0x1A3A5C : 0x2D3748;
    draw_rounded_rect(ctrl_x, ypos, ctrl_w, row_h, theme_col);
    draw_rounded_rect_border(ctrl_x, ypos, ctrl_w, row_h, COLOR_BORDER);
    draw_text(ctrl_x+8, ypos+22, settings.dark_mode ? "Dark" : "Light",
              settings.dark_mode ? COLOR_ACCENT : COLOR_ACCENT_ORANGE);
    ypos += row_h + 8;

    draw_text(label_x, ypos+12, "Preview:", COLOR_TEXT);
    unsigned long prev_col = settings.show_preview ? 0x1A3A5C : 0x2D3748;
    draw_rounded_rect(ctrl_x, ypos, ctrl_w, row_h, prev_col);
    draw_rounded_rect_border(ctrl_x, ypos, ctrl_w, row_h, COLOR_BORDER);
    draw_text(ctrl_x+8, ypos+22, settings.show_preview ? "ON" : "OFF",
              settings.show_preview ? COLOR_ACCENT_GREEN : COLOR_TEXT_SEC);
    ypos += row_h + 8;

    draw_text(label_x, ypos+12, "Auto Video:", COLOR_TEXT);
    unsigned long auto_col = settings.auto_open_video ? 0x1A3A5C : 0x2D3748;
    draw_rounded_rect(ctrl_x, ypos, ctrl_w, row_h, auto_col);
    draw_rounded_rect_border(ctrl_x, ypos, ctrl_w, row_h, COLOR_BORDER);
    draw_text(ctrl_x+8, ypos+22, settings.auto_open_video ? "ON" : "OFF",
              settings.auto_open_video ? COLOR_ACCENT_GREEN : COLOR_TEXT_SEC);

    draw_rounded_rect(sx + 70, sy + sh - 40, sw - 140, 30, COLOR_ACCENT);
    draw_text_bold(sx + sw/2 - 22, sy + sh - 20, "Close", COLOR_TEXT_BRIGHT);
}

static void draw_file_tree(void) {
    int hh = 50, fh = 46;
    int tree_w = settings.tree_width;
    int preview_x = tree_w + 6;
    int preview_w = win_width - tree_w - 12;
    int table_y = hh + 6;
    int table_h = win_height - table_y - fh - 10;

    draw_rect(0, 0, win_width, win_height, COLOR_BG);
    draw_rounded_rect(preview_x, table_y, preview_w, table_h, COLOR_PREVIEW_BG);
    draw_rounded_rect_border(preview_x, table_y, preview_w, table_h, COLOR_BORDER);

    if (settings.file_mode && settings.selected_file_index < flat_count) {
        FileNode *f = flat_list[settings.selected_file_index];
        if (f && f->is_text && preview_size > 0) {
            stop_video();
            clear_image_cache();
            int line_y = table_y + 20;
            char temp_content[1024*32];
            strncpy(temp_content, preview_content, sizeof(temp_content)-1);
            temp_content[sizeof(temp_content)-1] = '\0';
            char *line = strtok(temp_content, "\n");
            int line_num = 1;
            while (line && line_y < table_y + table_h - 10) {
                char num_str[16];
                snprintf(num_str, sizeof(num_str), "%4d ", line_num);
                draw_text(preview_x + 6, line_y, num_str, COLOR_LINE_NUM);
                draw_text(preview_x + 45, line_y, line, COLOR_TEXT);
                line = strtok(NULL, "\n");
                line_y += font_height + 4;
                line_num++;
            }
        } else if (f && f->is_image) {
            stop_video();
            load_image_cached(f->path);
            render_cached_image(preview_x + 10, table_y + 10,
                               preview_w - 20, table_h - 20);
        } else if (f && f->is_video) {
            clear_image_cache();
            if (settings.auto_open_video) {
                start_video(f->path, preview_x + 10, table_y + 10,
                           preview_w - 20, table_h - 20);
            } else {
                stop_video();
                draw_text(preview_x + 20, table_y + 40, "[ VIDEO FILE ]", COLOR_ACCENT_ORANGE);
                draw_text(preview_x + 20, table_y + 70, "File:", COLOR_TEXT_SEC);
                draw_text(preview_x + 70, table_y + 70, f->name, COLOR_TEXT_BRIGHT);
                draw_text(preview_x + 20, table_y + 100, "Size:", COLOR_TEXT_SEC);
                char size_str[32];
                if (f->size < 1024) snprintf(size_str, sizeof(size_str), "%ld B", f->size);
                else if (f->size < 1024*1024) snprintf(size_str, sizeof(size_str), "%.1f KB", f->size / 1024.0);
                else snprintf(size_str, sizeof(size_str), "%.1f MB", f->size / (1024.0*1024.0));
                draw_text(preview_x + 70, table_y + 100, size_str, COLOR_TEXT_BRIGHT);
                draw_text(preview_x + 20, table_y + 130, "Path:", COLOR_TEXT_SEC);
                draw_text(preview_x + 70, table_y + 130, f->path, COLOR_TEXT);
                draw_text(preview_x + 20, table_y + 170, "Press Enter to play", COLOR_ACCENT);
            }
        } else if (f && f->is_dir) {
            stop_video();
            clear_image_cache();
            draw_text(preview_x + 20, table_y + 40, "[ DIRECTORY ]", COLOR_FOLDER);
            draw_text(preview_x + 20, table_y + 70, "Name:", COLOR_TEXT_SEC);
            draw_text(preview_x + 70, table_y + 70, f->name, COLOR_TEXT_BRIGHT);
            draw_text(preview_x + 20, table_y + 100, "Path:", COLOR_TEXT_SEC);
            draw_text(preview_x + 70, table_y + 100, f->path, COLOR_TEXT);
        } else {
            stop_video();
            clear_image_cache();
            draw_text(preview_x + 20, table_y + 40, "Select a file", COLOR_TEXT_SEC);
        }
    } else {
        stop_video();
        clear_image_cache();
        draw_text(preview_x + 20, table_y + 40, "Select a file to preview", COLOR_TEXT_SEC);
    }

    draw_rounded_rect(0, 0, tree_w + 5, hh, COLOR_BG_HEADER);
    draw_rect(0, hh-2, tree_w + 5, 2, COLOR_ACCENT);
    draw_text_bold(10, hh-14, "FILE TREE", COLOR_ACCENT);
    draw_text(10, hh-14 + font_height + 2, settings.current_path, COLOR_TEXT_SEC);

    draw_rounded_rect(0, table_y, tree_w + 5, table_h, COLOR_BG_CARD);
    draw_rounded_rect_border(0, table_y, tree_w + 5, table_h, COLOR_BORDER);

    if (flat_count == 0) {
        draw_text(10, table_y + 30, "No files found", COLOR_TEXT_SEC);
        return;
    }

    int max_display = (table_h - 10) / row_height;
    if (max_display < 1) max_display = 1;

    int max_scroll = flat_count - max_display;
    if (max_scroll < 0) max_scroll = 0;
    if (settings.file_scroll_offset > max_scroll) settings.file_scroll_offset = max_scroll;
    if (settings.file_scroll_offset < 0) settings.file_scroll_offset = 0;

    int sb_x = tree_w - scrollbar_width - 3;
    int sb_y = table_y + 3;
    int sb_h = table_h - 6;
    draw_scrollbar(flat_count, max_display, settings.file_scroll_offset, sb_x, sb_y, sb_h);

    int yp = table_y + 6;
    int tree_display_w = tree_w - scrollbar_width - 10;

    for (int i = settings.file_scroll_offset;
         i < flat_count && (i - settings.file_scroll_offset) < max_display; i++) {
        FileNode *f = flat_list[i];
        if (!f) continue;

        unsigned long rc = (i - settings.file_scroll_offset) % 2 == 0 ? 0x0A0E14 : 0x11161D;
        draw_rect(4, yp-10, tree_display_w, row_height, rc);

        unsigned long tc = COLOR_TEXT;
        if (i == settings.selected_file_index) {
            draw_rect(4, yp-10, tree_display_w, row_height, COLOR_SELECTED);
            tc = COLOR_TEXT_BRIGHT;
        }

        char indent[64] = "";
        for (int d = 0; d < f->depth && d < 20; d++) {
            if (d == f->depth - 1) strcat(indent, "  ");
            else strcat(indent, "   ");
        }

        char icon[8] = "";
        if (f->is_dir) strcpy(icon, "[D]");
        else if (f->is_image) strcpy(icon, "[I]");
        else if (f->is_video) strcpy(icon, "[V]");
        else if (f->is_text) strcpy(icon, "[T]");
        else strcpy(icon, "[ ]");

        unsigned long name_color = COLOR_FILE;
        if (f->is_dir) name_color = COLOR_FOLDER;
        else if (f->is_image) name_color = COLOR_ACCENT_GREEN;
        else if (f->is_video) name_color = COLOR_ACCENT_ORANGE;

        char display_name[512];
        if (f->depth == 0) snprintf(display_name, sizeof(display_name), "%s %s", icon, f->name);
        else snprintf(display_name, sizeof(display_name), "%s%s %s", indent, icon, f->name);
        draw_text(12, yp, display_name, name_color);

        yp += row_height;
    }

    int fy = win_height - fh;
    draw_rect(0, fy, win_width, fh, COLOR_BG_CARD);
    draw_rect(0, fy, win_width, 1, COLOR_BORDER);

    char footer[256];
    int disp = settings.file_scroll_offset + max_display;
    if (disp > flat_count) disp = flat_count;
    snprintf(footer, sizeof(footer),
             "Files: %d  |  Showing: %d-%d  |  Click: Select  |  Enter: Open  |  Q:Quit",
             flat_count, settings.file_scroll_offset + 1, disp);
    draw_text(10, fy + 28, footer, COLOR_TEXT_SEC);
}

static void draw_process_monitor(void) {
    stop_video();
    clear_image_cache();

    int p = 16, hh = 50, fh = 46;
    int table_y = hh + 6;
    int table_h = win_height - table_y - fh - 10;
    if (table_h < 80) table_h = 80;

    draw_rect(0, 0, win_width, win_height, COLOR_BG);
    draw_rounded_rect(0, 0, win_width, hh, COLOR_BG_HEADER);
    draw_rect(0, hh-2, win_width, 2, COLOR_ACCENT);
    draw_text_bold(p+10, hh-14, "XRAY-SCOPE v1.0", COLOR_TEXT_BRIGHT);

    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
    draw_text(win_width-120, hh-14, ts, COLOR_TEXT_SEC);

    int sy = hh + 4;
    draw_rounded_rect(p, sy, win_width - 2*p - 12 - scrollbar_width, 32, COLOR_BG_CARD);
    draw_rounded_rect_border(p, sy, win_width - 2*p - 12 - scrollbar_width, 32, COLOR_BORDER);

    char info[128];
    char *user = getenv("USER");
    if (!user) user = "unknown";
    snprintf(info, sizeof(info), "Processes: %d  |  User: %s  |  Sorted: MEMORY",
             proc_data.count, user);
    draw_text(p+12, sy+22, info, COLOR_TEXT);

    draw_rounded_rect(p, table_y, win_width - 2*p - 12 - scrollbar_width, table_h, COLOR_BG_CARD);
    draw_rounded_rect_border(p, table_y, win_width - 2*p - 12 - scrollbar_width, table_h, COLOR_BORDER);

    int hy = table_y + 18;
    draw_text_bold(p+14, hy, "PID", COLOR_ACCENT);
    draw_text_bold(p+100, hy, "USER", COLOR_ACCENT);
    draw_text_bold(p+190, hy, "PROCESS", COLOR_ACCENT);
    draw_text_bold(p+350, hy, "CPU%", COLOR_ACCENT);
    draw_text_bold(p+430, hy, "MEM(MB)", COLOR_ACCENT_GREEN);
    draw_text_bold(p+520, hy, "STATE", COLOR_ACCENT);
    draw_text_bold(p+600, hy, "COMMAND", COLOR_ACCENT);
    draw_rect(p+8, hy+8, win_width - 2*p - 12 - scrollbar_width - 16, 1, COLOR_BORDER);

    if (proc_data.count == 0) {
        draw_text(p+14, hy+30, "No processes found", COLOR_TEXT_SEC);
        return;
    }

    ProcessInfo sorted[MAX_PROCESSES];
    sort_processes(sorted, proc_data.count);

    int cnt = proc_data.count;
    int max_display = (table_h - 40) / row_height;
    if (max_display < 1) max_display = 1;
    if (max_display > 30) max_display = 30;

    int max_scroll = cnt - max_display;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    if (scroll_offset < 0) scroll_offset = 0;

    int sb_x = win_width - scrollbar_width - 14;
    int sb_y = table_y + 3;
    int sb_h = table_h - 6;
    draw_scrollbar(cnt, max_display, scroll_offset, sb_x, sb_y, sb_h);

    int yp = hy + 22;
    int rw = win_width - 2*p - 12 - scrollbar_width - 30;

    for (int i = scroll_offset; i < cnt && (i - scroll_offset) < max_display; i++) {
        ProcessInfo *proc = &sorted[i];
        unsigned long rc = (i - scroll_offset) % 2 == 0 ? 0x0A0E14 : 0x11161D;
        draw_rect(p+8, yp-12, rw, row_height, rc);

        unsigned long tc = COLOR_TEXT;
        if (proc->pid == selected_pid) {
            draw_rect(p+8, yp-12, rw, row_height, COLOR_SELECTED);
            tc = COLOR_TEXT_BRIGHT;
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "%d", proc->pid);
        draw_text(p+14, yp, buf, tc);

        char user_name[25];
        strncpy(user_name, proc->user, 24); user_name[24] = '\0';
        draw_text(p+100, yp, user_name, COLOR_TEXT_SEC);

        char name[25];
        strncpy(name, proc->name, 24); name[24] = '\0';
        draw_text(p+190, yp, name, tc);

        int cpu = proc->cpu_usage;
        if (cpu > 100) cpu = 100;
        int bar_w = (cpu * 50) / 100;
        if (bar_w > 50) bar_w = 50;
        draw_rect(p+345, yp-6, bar_w, 10, COLOR_ACCENT);
        snprintf(buf, sizeof(buf), "%3d%%", cpu);
        draw_text(p+400, yp, buf, COLOR_ACCENT);

        long mem = proc->memory_usage / 1024;
        unsigned long mc = COLOR_ACCENT_GREEN;
        if (mem > 1000) mc = COLOR_ACCENT_ORANGE;
        if (mem > 5000) mc = COLOR_ACCENT_RED;
        snprintf(buf, sizeof(buf), "%5ld", mem);
        draw_text(p+430, yp, buf, mc);

        char st[5];
        unsigned long sc = COLOR_TEXT_SEC;
        switch(proc->state) {
            case 'R': strcpy(st, "RUN"); sc = COLOR_ACCENT_GREEN; break;
            case 'S': strcpy(st, "SLP"); sc = COLOR_ACCENT; break;
            case 'Z': strcpy(st, "ZMB"); sc = COLOR_ACCENT_ORANGE; break;
            case 'T': strcpy(st, "STP"); sc = COLOR_ACCENT_RED; break;
            default: snprintf(st, 5, "%c", proc->state); break;
        }
        draw_text(p+520, yp, st, sc);

        char cmd[24];
        strncpy(cmd, proc->cmdline, 22); cmd[22] = '\0';
        draw_text(p+600, yp, cmd, COLOR_TEXT_SEC);

        yp += row_height;
    }

    int fy = win_height - fh;
    draw_rect(0, fy, win_width, fh, COLOR_BG_CARD);
    draw_rect(0, fy, win_width, 1, COLOR_BORDER);

    char footer[256];
    int disp = scroll_offset + max_display;
    if (disp > cnt) disp = cnt;
    snprintf(footer, sizeof(footer),
             "Total: %d  |  Showing: %d-%d  |  Sorted: MEMORY  |  Q:Quit  Up/Down:Scroll",
             cnt, scroll_offset + 1, disp);
    draw_text(p+10, fy+28, footer, COLOR_TEXT_SEC);
}

static void render_gui(void) {
    if (!dpy || !win || !back_buffer) return;

    if (settings.file_mode) draw_file_tree();
    else draw_process_monitor();

    int btn_y = 12, btn_h = 32, btn_w = 76;

    files_btn_x = win_width - 300; files_btn_y = btn_y;
    draw_rounded_rect(files_btn_x, files_btn_y, btn_w, btn_h,
              settings.file_mode ? 0x1A3A5C : (files_btn_hover ? 0x2D3748 : 0x1C2333));
    draw_rounded_rect_border(files_btn_x, files_btn_y, btn_w, btn_h, COLOR_BORDER);
    draw_text(files_btn_x + 14, files_btn_y + 22, "Files",
              settings.file_mode ? COLOR_ACCENT : COLOR_TEXT_SEC);

    int proc_btn_x = win_width - 215;
    draw_rounded_rect(proc_btn_x, btn_y, btn_w, btn_h,
              !settings.file_mode ? 0x1A3A5C : (process_btn_hover ? 0x2D3748 : 0x1C2333));
    draw_rounded_rect_border(proc_btn_x, btn_y, btn_w, btn_h, COLOR_BORDER);
    draw_text(proc_btn_x + 8, btn_y + 22, "Process",
              !settings.file_mode ? COLOR_ACCENT : COLOR_TEXT_SEC);

    settings_btn_x = win_width - 130; settings_btn_y = btn_y;
    settings_btn_w = 80; settings_btn_h = btn_h;
    draw_rounded_rect(settings_btn_x, settings_btn_y, settings_btn_w, settings_btn_h,
              settings_hover ? 0x2D3748 : 0x1C2333);
    draw_rounded_rect_border(settings_btn_x, settings_btn_y, settings_btn_w, settings_btn_h, COLOR_BORDER);
    draw_text(settings_btn_x + 10, settings_btn_y + 22, "Settings", COLOR_TEXT_SEC);

    if (settings.show_settings) draw_settings_window();

    flush_back_buffer();
    raise_video_window();
}

static void handle_settings_click(int x, int y) {
    int sw = 380, sh = 280;
    int sx = (win_width - sw) / 2;
    int sy = (win_height - sh) / 2;

    if (x >= sx + 70 && x <= sx + sw - 70 &&
        y >= sy + sh - 40 && y <= sy + sh - 10) {
        settings.show_settings = 0; return;
    }

    int ypos = sy + 68;
    int ctrl_x = sx + 150;
    int ctrl_w = 190;
    int row_h = 32;

    if (x >= ctrl_x && x <= ctrl_x + ctrl_w && y >= ypos && y <= ypos + row_h) {
        settings.sleep_mode = !settings.sleep_mode; return;
    }
    ypos += row_h + 8;
    if (x >= ctrl_x && x <= ctrl_x + ctrl_w && y >= ypos && y <= ypos + row_h) {
        settings.dark_mode = !settings.dark_mode; update_theme_colors(); return;
    }
    ypos += row_h + 8;
    if (x >= ctrl_x && x <= ctrl_x + ctrl_w && y >= ypos && y <= ypos + row_h) {
        settings.show_preview = !settings.show_preview; return;
    }
    ypos += row_h + 8;
    if (x >= ctrl_x && x <= ctrl_x + ctrl_w && y >= ypos && y <= ypos + row_h) {
        settings.auto_open_video = !settings.auto_open_video;
        if (!settings.auto_open_video) stop_video();
        return;
    }
}

static void* collect_thread(void *arg) {
    (void)arg;
    while (running) {
        read_process_data();
        if (settings.sleep_mode) usleep(500000);
        else usleep(100000);
    }
    return NULL;
}

static void sig_handler(int sig) { (void)sig; running = false; }

int main(int argc, char **argv) {
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = true;
    }

    XInitThreads();

    printf("\n============================================================\n");
    printf("  XRAY-SCOPE v1.0 - System Monitor + File Tree\n");
    printf("============================================================\n\n");
    printf("Mode: %s\n", headless ? "HEADLESS" : "GUI");
    printf("Controls: Up/Down scroll, Enter to open, Q to quit\n\n");

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    memset(prev_cpu, 0, sizeof(prev_cpu));
    prev_system_total = read_system_cpu_total();
    first_run = 1;
    update_theme_colors();

    char *home = getenv("HOME");
    if (!home) home = "/home";

    file_tree.root = build_tree(home, 0);
    if (file_tree.root) {
        file_tree.root->expanded = 1;
        strcpy(settings.current_path, home);
    }
    update_tree_flat();

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

        imlib_context_set_display(dpy);
        imlib_context_set_visual(DefaultVisual(dpy, DefaultScreen(dpy)));
        imlib_context_set_colormap(DefaultColormap(dpy, DefaultScreen(dpy)));

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

        create_back_buffer();
        imlib_context_set_drawable(back_buffer);

        XStoreName(dpy, win, "XRAY-SCOPE");
        XMapWindow(dpy, win);
        gc = XCreateGC(dpy, win, 0, NULL);
        load_fonts();

        XEvent ev;
        int timeout = 100;
        while (timeout-- > 0) {
            if (XCheckTypedWindowEvent(dpy, win, MapNotify, &ev)) break;
            usleep(10000);
        }

        printf("Running... (Press Ctrl+C or 'q' to stop)\n\n");

        while (running) {
            while (XPending(dpy) > 0) {
                XNextEvent(dpy, &ev);

                if (ev.type == KeyPress) {
                    KeySym key = XLookupKeysym(&ev.xkey, 0);

                    if (key == XK_q || key == XK_Q || key == XK_Escape) {
                        running = false; break;
                    }

                    if (key == XK_Up) {
                        if (settings.file_mode) {
                            if (settings.file_scroll_offset > 0) settings.file_scroll_offset--;
                            if (settings.selected_file_index > 0) settings.selected_file_index--;
                        } else { if (scroll_offset > 0) scroll_offset--; }
                    }

                    if (key == XK_Down) {
                        if (settings.file_mode) {
                            int ms = flat_count - 20; if (ms < 0) ms = 0;
                            if (settings.file_scroll_offset < ms) settings.file_scroll_offset++;
                            if (settings.selected_file_index < flat_count - 1) settings.selected_file_index++;
                        } else {
                            int ms = proc_data.count - 20; if (ms < 0) ms = 0;
                            if (scroll_offset < ms) scroll_offset++;
                        }
                    }

                    if (key == XK_Return) {
                        if (settings.file_mode && settings.selected_file_index < flat_count) {
                            FileNode *f = flat_list[settings.selected_file_index];
                            if (f && f->is_dir) {
                                f->expanded = !f->expanded;
                                update_tree_flat();
                                settings.file_scroll_offset = 0;
                            } else if (f && f->is_text) {
                                load_file_content(f->path);
                            } else if (f && f->is_video) {
                                int px = settings.tree_width + 6;
                                int pw = win_width - settings.tree_width - 12;
                                int ty = 50 + 6;
                                int th = win_height - ty - 46 - 10;
                                start_video(f->path, px + 10, ty + 10, pw - 20, th - 20);
                            }
                        }
                    }
                }

                if (ev.type == ConfigureNotify) {
                    if (ev.xconfigure.width != win_width || ev.xconfigure.height != win_height) {
                        win_width = ev.xconfigure.width;
                        win_height = ev.xconfigure.height;
                        create_back_buffer();
                        stop_video();
                    }
                }

                if (ev.type == MotionNotify) {
                    int x = ev.xmotion.x;
                    int y = ev.xmotion.y;
                    settings_hover = (x >= settings_btn_x && x <= settings_btn_x + settings_btn_w &&
                                      y >= settings_btn_y && y <= settings_btn_y + settings_btn_h);
                    files_btn_hover = (x >= files_btn_x && x <= files_btn_x + files_btn_w &&
                                       y >= files_btn_y && y <= files_btn_y + files_btn_h);
                    process_btn_hover = (x >= win_width - 215 && x <= win_width - 215 + files_btn_w &&
                                         y >= files_btn_y && y <= files_btn_y + files_btn_h);
                }

                if (ev.type == ButtonPress) {
                    int x = ev.xbutton.x;
                    int y = ev.xbutton.y;

                    if (ev.xbutton.button == 4) {
                        if (settings.file_mode) {
                            if (settings.file_scroll_offset > 0) settings.file_scroll_offset--;
                            if (settings.selected_file_index > 0) settings.selected_file_index--;
                        } else { if (scroll_offset > 0) scroll_offset--; }
                        continue;
                    }
                    if (ev.xbutton.button == 5) {
                        if (settings.file_mode) {
                            int ms = flat_count - 20; if (ms < 0) ms = 0;
                            if (settings.file_scroll_offset < ms) settings.file_scroll_offset++;
                            if (settings.selected_file_index < flat_count - 1) settings.selected_file_index++;
                        } else {
                            int ms = proc_data.count - 20; if (ms < 0) ms = 0;
                            if (scroll_offset < ms) scroll_offset++;
                        }
                        continue;
                    }

                    if (x >= files_btn_x && x <= files_btn_x + files_btn_w &&
                        y >= files_btn_y && y <= files_btn_y + files_btn_h) {
                        settings.file_mode = 1; continue;
                    }

                    int pbx = win_width - 215;
                    if (x >= pbx && x <= pbx + files_btn_w &&
                        y >= files_btn_y && y <= files_btn_y + files_btn_h) {
                        settings.file_mode = 0; continue;
                    }

                    if (x >= settings_btn_x && x <= settings_btn_x + settings_btn_w &&
                        y >= settings_btn_y && y <= settings_btn_y + settings_btn_h) {
                        settings.show_settings = !settings.show_settings; continue;
                    }

                    if (settings.show_settings) {
                        int sw = 380, sh = 280;
                        int sx = (win_width - sw) / 2;
                        int sy = (win_height - sh) / 2;
                        if (x >= sx && x <= sx + sw && y >= sy && y <= sy + sh) {
                            handle_settings_click(x, y); continue;
                        } else { settings.show_settings = 0; continue; }
                    }

                    if (settings.file_mode) {
                        int hh = 50;
                        int ty = hh + 6;
                        int hy = ty + 6;
                        int idx = (y - hy) / row_height + settings.file_scroll_offset;
                        if (idx >= 0 && idx < flat_count) {
                            settings.selected_file_index = idx;
                            FileNode *f = flat_list[idx];
                            if (f && f->is_text) load_file_content(f->path);
                        }
                        continue;
                    }

                    int sbx = win_width - scrollbar_width - 14;
                    if (x >= sbx && x <= sbx + scrollbar_width) {
                        scrollbar_dragging = 1; continue;
                    }

                    if (!settings.file_mode) {
                        int ty = 50 + 6 + 18 + 22;
                        int idx = (y - ty) / row_height + scroll_offset;
                        if (idx >= 0 && idx < proc_data.count) {
                            ProcessInfo sorted[MAX_PROCESSES];
                            sort_processes(sorted, proc_data.count);
                            if (idx < proc_data.count) selected_pid = sorted[idx].pid;
                        }
                    }
                }

                if (ev.type == ButtonRelease) scrollbar_dragging = 0;

                if (ev.type == MotionNotify && scrollbar_dragging) {
                    int sby = 180;
                    int sbh = win_height - 260;
                    int tr = settings.file_mode ? flat_count : proc_data.count;
                    int vr = 20;
                    int ms = tr - vr; if (ms < 0) ms = 0;
                    int th = sbh * vr / tr; if (th < 20) th = 20;
                    int *off = settings.file_mode ? &settings.file_scroll_offset : &scroll_offset;
                    int no = (ev.xmotion.y - sby - th/2) * ms / (sbh - th);
                    if (no < 0) no = 0;
                    if (no > ms) no = ms;
                    *off = no;
                }
            }

            render_gui();
            usleep(30000);
        }

        stop_video();
        clear_image_cache();

        if (font && font != font_bold) XFreeFont(dpy, font);
        if (font_bold && font_bold != font) XFreeFont(dpy, font_bold);
        if (gc) XFreeGC(dpy, gc);
        if (back_buffer) XFreePixmap(dpy, back_buffer);
        if (win) XDestroyWindow(dpy, win);
        if (dpy) XCloseDisplay(dpy);
    }

    running = false;
    pthread_join(thread, NULL);
    if (file_tree.root) free_tree(file_tree.root);
    if (flat_list) free(flat_list);
    printf("\nXRAY-SCOPE stopped successfully\n");
    return 0;
}

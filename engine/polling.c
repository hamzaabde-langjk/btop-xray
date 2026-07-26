#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <pwd.h>
#include "adapter.h"

typedef struct {
    int running;
    SystemEvent *event_buffer;
    int buffer_size;
    int event_count;
    pthread_mutex_t lock;
    unsigned long long last_cpu_time[8192];
    unsigned long long last_system_time;
    int first_run;
} PollingEngine;

static PollingEngine engine = {
    .running = 0,
    .event_buffer = NULL,
    .buffer_size = 4096,
    .event_count = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .last_system_time = 0,
    .first_run = 1
};

static char* read_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    char *buf = malloc(1024);
    if (!buf) { close(fd); return NULL; }
    ssize_t n = read(fd, buf, 1023);
    close(fd);
    if (n <= 0) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

static unsigned long long get_system_cpu_time(void) {
    char *content = read_file("/proc/stat");
    if (!content) return 0;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (sscanf(content, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
        free(content);
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }
    free(content);
    return 0;
}

static unsigned long long get_process_cpu_time(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char *content = read_file(path);
    if (!content) return 0;
    unsigned long long utime = 0, stime = 0;
    char *token = strtok(content, " ");
    int field = 0;
    while (token && field < 15) {
        if (field == 13) utime = strtoull(token, NULL, 10);
        if (field == 14) stime = strtoull(token, NULL, 10);
        token = strtok(NULL, " ");
        field++;
    }
    free(content);
    return utime + stime;
}

static void get_process_name(int pid, char *name, size_t size) {
    char path[256];
    char *content;
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    content = read_file(path);
    if (content) {
        char *p = content;
        while (*p && *p != '\n') p++;
        *p = '\0';
        if (strlen(content) > 0) {
            strncpy(name, content, size - 1);
            name[size - 1] = '\0';
            free(content);
            return;
        }
        free(content);
    }
    snprintf(name, size, "pid_%d", pid);
}

static void get_process_user(int pid, char *user, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    char *content = read_file(path);
    if (content) {
        char *line = strstr(content, "Uid:");
        if (line) {
            unsigned int uid;
            if (sscanf(line, "Uid: %u", &uid) == 1) {
                struct passwd *pw = getpwuid(uid);
                if (pw) {
                    strncpy(user, pw->pw_name, size - 1);
                    user[size - 1] = '\0';
                    free(content);
                    return;
                }
            }
        }
        free(content);
    }
    snprintf(user, size, "user");
}

static void get_process_cmdline(int pid, char *cmdline, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    char *content = read_file(path);
    if (content && strlen(content) > 0) {
        size_t len = 0;
        for (size_t i = 0; i < size - 1 && content[i]; i++) {
            if (content[i] == '\0') content[i] = ' ';
            len = i + 1;
        }
        content[len] = '\0';
        strncpy(cmdline, content, size - 1);
        cmdline[size - 1] = '\0';
        free(content);
        return;
    }
    if (content) free(content);
    snprintf(cmdline, size, "[kernel]");
}

static long get_process_memory(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/statm", pid);
    char *content = read_file(path);
    if (!content) return 0;
    long resident;
    if (sscanf(content, "%*ld %ld", &resident) == 1) {
        free(content);
        return resident * 4;
    }
    free(content);
    return 0;
}

static char get_process_state(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char *content = read_file(path);
    if (!content) return '?';
    char state = '?';
    char *token = strtok(content, " ");
    int field = 0;
    while (token && field < 3) {
        if (field == 2) { 
            state = token[0]; 
            break; 
        }
        token = strtok(NULL, " ");
        field++;
    }
    free(content);
    return state;
}

static long get_process_threads(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char *content = read_file(path);
    if (!content) return 0;
    long threads = 0;
    char *token = strtok(content, " ");
    int field = 0;
    while (token && field < 20) {
        if (field == 19) { threads = atol(token); break; }
        token = strtok(NULL, " ");
        field++;
    }
    free(content);
    return threads;
}

static int get_process_ppid(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char *content = read_file(path);
    if (!content) return 0;
    int ppid = 0;
    char *token = strtok(content, " ");
    int field = 0;
    while (token && field < 4) {
        if (field == 3) { ppid = atoi(token); break; }
        token = strtok(NULL, " ");
        field++;
    }
    free(content);
    return ppid;
}

static int is_kernel_thread(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    char *content = read_file(path);
    if (content) {
        if (content[0] == '[') {
            free(content);
            return 1;
        }
        free(content);
    }
    return 0;
}

static bool get_process_info(int pid, ProcessInfo *info) {
    if (is_kernel_thread(pid)) {
        return false;
    }
    
    memset(info, 0, sizeof(ProcessInfo));
    info->pid = pid;
    get_process_name(pid, info->name, sizeof(info->name));
    info->state = get_process_state(pid);
    info->memory_usage = get_process_memory(pid);
    info->num_threads = get_process_threads(pid);
    info->ppid = get_process_ppid(pid);
    get_process_user(pid, info->user, sizeof(info->user));
    get_process_cmdline(pid, info->cmdline, sizeof(info->cmdline));
    
    unsigned long long process_time = get_process_cpu_time(pid);
    unsigned long long system_time = get_system_cpu_time();
    
    if (!engine.first_run && engine.last_cpu_time[pid % 8192] > 0) {
        unsigned long long time_diff = system_time - engine.last_system_time;
        unsigned long long process_diff = process_time - engine.last_cpu_time[pid % 8192];
        
        if (time_diff > 0) {
            info->cpu_usage = (process_diff * 100) / time_diff;
            if (info->cpu_usage > 100) info->cpu_usage = 100;
        } else {
            info->cpu_usage = 0;
        }
    } else {
        info->cpu_usage = 0;
    }
    
    engine.last_cpu_time[pid % 8192] = process_time;
    engine.last_system_time = system_time;
    info->last_update = time(NULL);
    
    return true;
}

static int collect_processes(ProcessInfo *buffer, int max_count) {
    DIR *dir = opendir("/proc");
    if (!dir) return 0;
    int count = 0;
    struct dirent *entry;
    int pids[4096];
    int num_pids = 0;
    
    while ((entry = readdir(dir)) != NULL && num_pids < 4096) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        int pid = atoi(entry->d_name);
        if (pid > 0) pids[num_pids++] = pid;
    }
    closedir(dir);
    
    for (int i = 0; i < num_pids && count < max_count; i++) {
        if (get_process_info(pids[i], &buffer[count])) {
            count++;
        }
    }
    engine.first_run = 0;
    return count;
}

static bool polling_init(void) {
    engine.event_buffer = malloc(engine.buffer_size * sizeof(SystemEvent));
    if (!engine.event_buffer) return false;
    memset(engine.last_cpu_time, 0, sizeof(engine.last_cpu_time));
    engine.last_system_time = get_system_cpu_time();
    engine.first_run = 1;
    engine.running = 1;
    return true;
}

static int polling_get_processes(ProcessInfo *buffer, int max_count) {
    return collect_processes(buffer, max_count);
}

static int polling_poll_events(SystemEvent *buffer, int max_count) {
    pthread_mutex_lock(&engine.lock);
    int count = engine.event_count;
    if (count > max_count) count = max_count;
    memcpy(buffer, engine.event_buffer, count * sizeof(SystemEvent));
    engine.event_count = 0;
    pthread_mutex_unlock(&engine.lock);
    return count;
}

static void polling_cleanup(void) {
    engine.running = 0;
    if (engine.event_buffer) { free(engine.event_buffer); engine.event_buffer = NULL; }
}

EngineAdapter adapter = {
    .init = polling_init,
    .get_processes = polling_get_processes,
    .poll_events = polling_poll_events,
    .cleanup = polling_cleanup,
    .name = "polling",
    .events_per_second = 10000,
    .memory_usage = 6 * 1024 * 1024
};

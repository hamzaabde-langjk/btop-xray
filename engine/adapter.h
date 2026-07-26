#ifndef ENGINE_ADAPTER_H
#define ENGINE_ADAPTER_H
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
typedef struct {
    char name[64];
    int pid;
    int ppid;
    char cmdline[512];
    char cwd[256];
    char user[64];
    char state;
    long cpu_usage;
    long memory_usage;
    unsigned long long io_read;
    unsigned long long io_write;
    int connections[32];
    int conn_count;
    uint64_t inodes[64];
    int inode_count;
    double latitude;
    double longitude;
    uint64_t last_update;
    unsigned long long utime;
    unsigned long long stime;
    unsigned long long start_time;
    long nice;
    long priority;
    long num_threads;
    unsigned long long vsize;
    long rss;
    unsigned long long cutime;
    unsigned long long cstime;
} ProcessInfo;
typedef struct {
    uint64_t timestamp;
    uint32_t pid;
    uint32_t event_type;
    uint64_t inode;
    char path[256];
    char remote_ip[16];
    uint16_t remote_port;
    uint64_t bytes_transferred;
} SystemEvent;
typedef struct EngineAdapter {
    bool (*init)(void);
    int (*get_processes)(ProcessInfo *buffer, int max_count);
    int (*poll_events)(SystemEvent *buffer, int max_count);
    void (*cleanup)(void);
    const char *name;
    uint32_t events_per_second;
    uint32_t memory_usage;
} EngineAdapter;
extern EngineAdapter adapter;
#endif

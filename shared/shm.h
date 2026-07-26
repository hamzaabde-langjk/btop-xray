#ifndef SHARED_SHM_H
#define SHARED_SHM_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdatomic.h>
#include "../engine/adapter.h"
typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t num_processes;
    uint32_t num_events;
    uint64_t timestamp;
    ProcessInfo processes[4096];
    SystemEvent events[16384];
    uint32_t process_count;
    uint32_t event_count;
    atomic_flag lock;
    char reserved[1024];
} SharedMemoryHeader;
void* shm_init(key_t key, size_t size);
void* shm_get(key_t key);
void shm_destroy(void *ptr);
size_t shm_get_size(key_t key);
#endif

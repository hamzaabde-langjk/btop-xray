#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <errno.h>
#include "shm.h"

void* shm_init(key_t key, size_t size) {
    int shmid = shmget(key, size, IPC_CREAT | 0666);
    if (shmid < 0) {
        shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0666);
        if (shmid < 0) {
            perror("shmget");
            return NULL;
        }
    }
    void *ptr = shmat(shmid, NULL, 0);
    if (ptr == (void*)-1) {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        return NULL;
    }
    memset(ptr, 0, size);
    return ptr;
}

void* shm_get(key_t key) {
    int shmid = shmget(key, 0, 0);
    if (shmid < 0) return NULL;
    return shmat(shmid, NULL, 0);
}

void shm_destroy(void *ptr) {
    if (!ptr) return;
    shmdt(ptr);
}

size_t shm_get_size(key_t key) {
    int shmid = shmget(key, 0, 0);
    if (shmid < 0) return 0;
    struct shmid_ds ds;
    if (shmctl(shmid, IPC_STAT, &ds) < 0) return 0;
    return ds.shm_segsz;
}

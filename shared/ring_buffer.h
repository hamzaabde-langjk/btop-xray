#ifndef SHARED_RING_BUFFER_H
#define SHARED_RING_BUFFER_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    void *buffer;
    size_t element_size;
    size_t capacity;
    volatile uint64_t head;
    volatile uint64_t tail;
    volatile uint64_t count;
} RingBuffer;
RingBuffer* ring_buffer_create(size_t capacity, size_t element_size);
void ring_buffer_destroy(RingBuffer *ring);
bool ring_buffer_push(RingBuffer *ring, const void *element);
bool ring_buffer_pop(RingBuffer *ring, void *element);
size_t ring_buffer_count(const RingBuffer *ring);
bool ring_buffer_empty(const RingBuffer *ring);
#endif

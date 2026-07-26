#include <stdlib.h>
#include <string.h>
#include "ring_buffer.h"

RingBuffer* ring_buffer_create(size_t capacity, size_t element_size) {
    RingBuffer *ring = malloc(sizeof(RingBuffer));
    if (!ring) return NULL;
    ring->buffer = malloc(capacity * element_size);
    if (!ring->buffer) { free(ring); return NULL; }
    ring->element_size = element_size;
    ring->capacity = capacity;
    ring->head = 0; ring->tail = 0; ring->count = 0;
    return ring;
}

void ring_buffer_destroy(RingBuffer *ring) {
    if (!ring) return;
    if (ring->buffer) free(ring->buffer);
    free(ring);
}

bool ring_buffer_push(RingBuffer *ring, const void *element) {
    if (!ring || !element || ring->count >= ring->capacity) return false;
    size_t pos = ring->tail % ring->capacity;
    memcpy((char*)ring->buffer + pos * ring->element_size, element, ring->element_size);
    ring->tail++; ring->count++;
    return true;
}

bool ring_buffer_pop(RingBuffer *ring, void *element) {
    if (!ring || !element || ring->count == 0) return false;
    size_t pos = ring->head % ring->capacity;
    memcpy(element, (char*)ring->buffer + pos * ring->element_size, ring->element_size);
    ring->head++; ring->count--;
    return true;
}

size_t ring_buffer_count(const RingBuffer *ring) { return ring ? ring->count : 0; }
bool ring_buffer_empty(const RingBuffer *ring) { return ring ? ring->count == 0 : true; }

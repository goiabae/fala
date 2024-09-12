//// HEADER-ONLY LIBRARY. To include impl define FALA_RING_IMPL

#ifndef FALA_RING_H
#define FALA_RING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ring {
	char* buf;
	size_t read;
	size_t write;
	size_t len;
	size_t cap;
} Ring;

// arbitrary choice
#define RING_DEFAULT_CAP 1024

Ring ring_init(void);
void ring_deinit(Ring* ring);
char ring_read(Ring* ring);
char ring_peek(Ring* ring);
void ring_write(Ring* ring, char c);
void ring_write_many(Ring* ring, char* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif

#ifdef FALA_RING_IMPL

#ifdef __cplusplus
extern "C" {
#endif

Ring ring_init(void) {
	Ring ring;
	ring.buf = (char*)malloc(sizeof(char) * RING_DEFAULT_CAP);
	ring.read = 0;
	ring.write = 0;
	ring.len = 0;
	ring.cap = RING_DEFAULT_CAP;
	return ring;
}

void ring_deinit(Ring* ring) { free(ring->buf); }

char ring_read(Ring* ring) {
	if (ring->len == 0) return -1;
	size_t idx = ring->read;
	assert(idx <= (ring->cap - 1));
	ring->read = (ring->read + 1) % ring->cap;
	ring->len = (ring->len > 0) ? ring->len - 1 : 0;
	char c = ring->buf[idx];
	return c;
}

char ring_peek(Ring* ring) {
	size_t idx = ring->read;
	assert(idx <= (ring->cap - 1));
	char c = ring->buf[idx];
	return c;
}

void ring_write(Ring* ring, char c) {
	size_t idx = ring->write;
	assert(idx <= (ring->cap - 1));
	ring->write = (ring->write + 1) % ring->cap;
	ring->len = (ring->len < ring->cap) ? ring->len + 1 : ring->cap;
	ring->buf[idx] = c;
}

void ring_write_many(Ring* ring, char* buf, size_t len) {
	assert(len <= (ring->cap - ring->len));
	for (size_t i = 0; i < len; i++) ring_write(ring, buf[i]);
}

#ifdef __cplusplus
}
#endif

#endif

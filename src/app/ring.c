#include <stdlib.h>
#include "ring.h"


void ring_init(Ring *r, int cap) {
r->buf = (char**)calloc((size_t)cap, sizeof(char*));
r->head = 0;
r->size = 0;
r->cap = cap;
}


void ring_clear_free(Ring *r) {
for (int i = 0; i < r->size; ++i) {
int idx = (r->head + i) % r->cap;
free(r->buf[idx]);
r->buf[idx] = NULL;
}
r->head = 0;
r->size = 0;
}


void ring_free(Ring *r) {
ring_clear_free(r);
free(r->buf);
r->buf = NULL;
r->cap = 0;
}


void ring_push_back(Ring *r, char *url) {
if (r->cap <= 0) { free(url); return; }
if (r->size == r->cap) {
int old = r->head;
free(r->buf[old]);
r->buf[old] = NULL;
r->head = (r->head + 1) % r->cap; // size stays = cap
} else {
r->size++;
}
int tail = (r->head + r->size - 1) % r->cap;
r->buf[tail] = url;
}


char *ring_pop_back(Ring *r) {
if (r->size == 0) return NULL;
int tail = (r->head + r->size - 1) % r->cap;
char *p = r->buf[tail];
r->buf[tail] = NULL;
r->size--;
if (r->size == 0) r->head = 0;
return p;
}


char *ring_at(const Ring *r, int i) {
int idx = (r->head + i) % r->cap;
return r->buf[idx];
}
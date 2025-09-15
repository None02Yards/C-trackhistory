#ifndef RING_H
#define RING_H


typedef struct {
char **buf;
int head; // index of oldest
int size; // number of items
int cap; // capacity (max items)
} Ring;


void ring_init(Ring *r, int cap);
void ring_clear_free(Ring *r); // frees all stored char* and clears
void ring_free(Ring *r); // clear + free buffer


void ring_push_back(Ring *r, char *url); // takes ownership, drops oldest if full
char *ring_pop_back(Ring *r); // returns ownership of newest
char *ring_at(const Ring *r, int i); // i=0..size-1 oldest..newest


#endif // RING_H
#include <stdlib.h>
#include "browser.h"
#include "util.h"


void browser_init(Browser *b, const char *homepage, int back_cap) {
b->current = sdup(homepage);
ring_init(&b->back, back_cap);
vec_init(&b->fwd);
}


void browser_destroy(Browser *b) {
free(b->current);
ring_free(&b->back);
vec_clear_free(&b->fwd);
vec_free(&b->fwd);
}


const char *browser_visit(Browser *b, const char *url) {
ring_push_back(&b->back, b->current);
vec_clear_free(&b->fwd);
b->current = sdup(url);
return b->current;
}


const char *browser_back(Browser *b, int steps) {
while (steps-- > 0) {
char *prev = ring_pop_back(&b->back);
if (!prev) break;
vec_push(&b->fwd, b->current);
b->current = prev;
}
return b->current;
}


const char *browser_forward(Browser *b, int steps) {
while (steps-- > 0) {
char *next = vec_pop(&b->fwd);
if (!next) break;
ring_push_back(&b->back, b->current);
b->current = next;
}
return b->current;
}


const char *browser_current(const Browser *b) { return b->current; }
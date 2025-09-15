#include <stdlib.h>
#include "vec.h"


void vec_init(Vec *v) { v->data = NULL; v->size = 0; v->cap = 0; }


void vec_reserve(Vec *v, int need) {
if (v->cap >= need) return;
int c = v->cap ? v->cap : 8;
while (c < need) c <<= 1;
v->data = (char**)realloc(v->data, (size_t)c * sizeof(char*));
v->cap = c;
}


void vec_push(Vec *v, char *p) {
vec_reserve(v, v->size + 1);
v->data[v->size++] = p;
}


char *vec_pop(Vec *v) {
if (v->size == 0) return NULL;
return v->data[--v->size];
}


void vec_clear_free(Vec *v) {
for (int i = 0; i < v->size; ++i) free(v->data[i]);
v->size = 0;
}


void vec_free(Vec *v) { free(v->data); v->data = NULL; v->cap = 0; v->size = 0; }
#ifndef VEC_H
#define VEC_H


typedef struct {
char **data;
int size, cap;
} Vec;


void vec_init(Vec *v);
void vec_reserve(Vec *v, int need);
void vec_push(Vec *v, char *p); // takes ownership
char *vec_pop(Vec *v); // returns ownership
void vec_clear_free(Vec *v);
void vec_free(Vec *v);


#endif // VEC_H
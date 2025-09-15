#ifndef BROWSER_H
#define BROWSER_H


#include "ring.h"
#include "vec.h"


typedef struct {
char *current;
Ring back; // capped ring buffer
Vec fwd; // forward stack
} Browser;


void browser_init(Browser *b, const char *homepage, int back_cap);
void browser_destroy(Browser *b);
const char *browser_visit(Browser *b, const char *url);
const char *browser_back(Browser *b, int steps);
const char *browser_forward(Browser *b, int steps);
const char *browser_current(const Browser *b);


#endif // BROWSER_H
#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include "util.h"
#include "vec.h"
#include <stdio.h>
typedef struct { char *name; char *url; } BMItem;

typedef struct {
    BMItem *data;
    int size, cap;
} BMList;

void bm_init(BMList *bm);
void bm_destroy(BMList *bm);
int  bm_add(BMList *bm, const char *name, const char *url);  // returns index
int  bm_find_by_name(const BMList *bm, const char *name);     // -1 if not found
const BMItem* bm_get(const BMList *bm, int idx);

// --- session helpers (JSON) ---
void bm_save_json(FILE *f, const BMList *bm);                 // writes [...], no key
int  bm_load_json_array(const char *json, size_t n, BMList *bm); // parse [...], replace contents

#endif

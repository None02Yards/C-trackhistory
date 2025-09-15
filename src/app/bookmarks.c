#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bookmarks.h"

// --- internal helpers ---
static void bm_reserve(BMList *bm, int need){
    if (bm->cap >= need) return;
    int c = bm->cap ? bm->cap : 8;
    while (c < need) c <<= 1;
    bm->data = (BMItem*)realloc(bm->data, (size_t)c * sizeof(BMItem));
    bm->cap = c;
}

void bm_init(BMList *bm){ bm->data=NULL; bm->size=0; bm->cap=0; }

void bm_destroy(BMList *bm){
    for (int i=0;i<bm->size;++i){ free(bm->data[i].name); free(bm->data[i].url); }
    free(bm->data); bm->data=NULL; bm->size=bm->cap=0;
}

int bm_add(BMList *bm, const char *name, const char *url){
    bm_reserve(bm, bm->size+1);
    bm->data[bm->size].name = sdup(name?name:"");
    bm->data[bm->size].url  = sdup(url?url:"");
    return bm->size++;
}

int bm_find_by_name(const BMList *bm, const char *name){
    if (!name) return -1;
    for (int i=0;i<bm->size;++i) if (strcmp(bm->data[i].name, name)==0) return i;
    return -1;
}

const BMItem* bm_get(const BMList *bm, int idx){
    if (idx < 0 || idx >= bm->size) return NULL;
    return &bm->data[idx];
}

// ---- JSON I/O ----
// We reuse a simple writer: write: [{"name":"..","url":".."},...]
static void json_escape_str(FILE *f, const char *s){
    fputc('"', f);
    for (const unsigned char *p=(const unsigned char*)s; *p; ++p){
        unsigned char c=*p;
        switch(c){
            case '\\': fputs("\\\\", f); break;
            case '\"': fputs("\\\"", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default: if (c<0x20) fprintf(f,"\\u%04x",c); else fputc(c,f);
        }
    }
    fputc('"', f);
}

void bm_save_json(FILE *f, const BMList *bm){
    fputc('[', f);
    for (int i=0;i<bm->size;++i){
        if (i) fputc(',', f);
        fputc('{', f);
        fputs("\"name\":", f); json_escape_str(f, bm->data[i].name);
        fputs(",\"url\":", f);  json_escape_str(f, bm->data[i].url);
        fputc('}', f);
    }
    fputc(']', f);
}

// Minimal parser for array of {"name":..,"url":..}. We’ll keep it simple:
// feed it the bytes exactly as we wrote them (session loader passes correct slice).
// Returns 1 on success.
int bm_load_json_array(const char *json, size_t n, BMList *bm){
    // Very small permissive reader: scan for "name":"...","url":"..." pairs in order.
    // For robustness you could reuse your JIn mini-parser; here we keep it tight.
    bm_destroy(bm); bm_init(bm);
    const char *p=json, *end=json+n;
    while (p<end){
        const char *name_key = strstr(p, "\"name\"");
        if (!name_key) break;
        const char *q = strchr(name_key, '"'); if (!q) break; // "
        q = strchr(q+1, '"'); if (!q) break;                   // end "name"
        const char *colon = strchr(q, ':'); if (!colon) break;
        const char *v1 = strchr(colon, '"'); if (!v1) break;
        const char *v1e = v1+1; while (v1e<end && !( *v1e=='"' && v1e[-1] != '\\')) v1e++;
        if (v1e>=end) break;

        const char *url_key = strstr(v1e, "\"url\"");
        if (!url_key) break;
        q = strchr(url_key, '"'); if (!q) break;
        q = strchr(q+1, '"'); if (!q) break;
        colon = strchr(q, ':'); if (!colon) break;
        const char *v2 = strchr(colon, '"'); if (!v2) break;
        const char *v2e = v2+1; while (v2e<end && !( *v2e=='"' && v2e[-1] != '\\')) v2e++;
        if (v2e>=end) break;

        // extract raw substrings (no unescape to keep it short)
        size_t nlen = (size_t)(v1e - (v1+1));
        size_t ulen = (size_t)(v2e - (v2+1));
        char *name = (char*)malloc(nlen+1); memcpy(name, v1+1, nlen); name[nlen]='\0';
        char *url  = (char*)malloc(ulen+1); memcpy(url,  v2+1, ulen); url [ulen]='\0';
        bm_add(bm, name, url);
        free(name); free(url);

        p = v2e+1;
    }
    return 1;
}

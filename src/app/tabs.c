#include <stdlib.h>
#include "bookmarks.h"
#include "tabs.h"


static void tm_reserve(TabManager *tm, int need) {
if (tm->cap >= need) return;
int c = tm->cap ? tm->cap : 4;
while (c < need) c <<= 1;
tm->tabs = (Browser**)realloc(tm->tabs, (size_t)c * sizeof(Browser*));
tm->cap = c;
}


void tm_init(TabManager *tm, int back_cap_default) {
tm->tabs = NULL; tm->count = 0; tm->cap = 0;
tm->active = -1;
tm->back_cap_default = back_cap_default;
   vec_init(&tm->closed_json);
    tm->autosave = 0; tm->autosave_path[0] = '\0';
    bm_init(&tm->bookmarks);   

}


int tm_new_tab(TabManager *tm, const char *homepage) {
tm_reserve(tm, tm->count + 1);
Browser *b = (Browser*)malloc(sizeof(Browser));
browser_init(b, homepage, tm->back_cap_default);
tm->tabs[tm->count] = b;
int id = tm->count;
tm->count++;
if (tm->active == -1) tm->active = id;
return id;
}


void tm_close_tab(TabManager *tm, int id) {
if (id < 0 || id >= tm->count) return;
browser_destroy(tm->tabs[id]);
free(tm->tabs[id]);
for (int i = id + 1; i < tm->count; ++i) tm->tabs[i-1] = tm->tabs[i];
tm->count--;
if (tm->count == 0) { tm->active = -1; return; }
if (tm->active == id) tm->active = (id >= tm->count) ? (tm->count - 1) : id;
else if (tm->active > id) tm->active--;
}


void tm_switch(TabManager *tm, int id) {
if (id < 0 || id >= tm->count) return;
tm->active = id;
}


Browser *tm_active(TabManager *tm) {
if (tm->active < 0 || tm->active >= tm->count) return NULL;
return tm->tabs[tm->active];
}


void tm_destroy(TabManager *tm) {
for (int i = 0; i < tm->count; ++i) {
browser_destroy(tm->tabs[i]);
free(tm->tabs[i]);
}
free(tm->tabs);
vec_clear_free(&tm->closed_json); vec_free(&tm->closed_json);
    bm_destroy(&tm->bookmarks);    // NEW
    tm->tabs=NULL; tm->cap=tm->count=0; tm->active=-1;  
}
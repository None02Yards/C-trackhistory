#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "features.h"
#include "session.h"   // single-tab helpers
#include "util.h"

// ---- Undo stack ----
void undo_init(UndoStack *u){ vec_init(&u->blobs); }
void undo_destroy(UndoStack *u){ vec_clear_free(&u->blobs); vec_free(&u->blobs); }

void undo_push_tab(UndoStack *u, const Browser *b) {
    char *obj = session_serialize_tab_json(b);  // JSON object: {"current":...,"back":[...],"forward":[...]}
    if (obj) vec_push(&u->blobs, obj);
}

int undo_reopen_top(UndoStack *u, TabManager *tm) {
    char *obj = vec_pop(&u->blobs);
    if (!obj) return -1;
    Browser *nb = NULL;
    if (!session_deserialize_tab_json(obj, &nb, tm->back_cap_default)) { free(obj); return -1; }
    free(obj);

    // Append new tab
    // reserve
    if (tm->cap <= tm->count) {
        int c = tm->cap ? tm->cap : 4; while (c <= tm->count) c <<= 1;
        tm->tabs = (Browser**)realloc(tm->tabs, (size_t)c * sizeof(Browser*)); tm->cap = c;
    }
    tm->tabs[tm->count] = nb;
    int id = tm->count++;
    tm->active = id;
    return id;
}

// ---- Autosave ----
void autosave_on(TabManager *tm, const char *path_or_null) {
    tm->autosave = 1;
    if (path_or_null && *path_or_null) {
        strncpy(tm->autosave_path, path_or_null, sizeof tm->autosave_path - 1);
        tm->autosave_path[sizeof tm->autosave_path - 1] = '\0';
    } else if (!tm->autosave_path[0]) {
        strcpy(tm->autosave_path, "session.json");
    }
}
void autosave_off(TabManager *tm) { tm->autosave = 0; }

void autosave_maybe(const TabManager *tm) {
    if (!tm->autosave) return;
    const char *p = tm->autosave_path[0] ? tm->autosave_path : "session.json";
    save_session_json(p, tm);
}

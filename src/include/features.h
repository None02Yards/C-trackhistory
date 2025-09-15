#ifndef FEATURES_H
#define FEATURES_H

#include "tabs.h"
#include "session.h"  


typedef struct {
    Vec blobs;  // vector<char*>
} UndoStack;

void undo_init(UndoStack *u);
void undo_destroy(UndoStack *u);
void undo_push_tab(UndoStack *u, const Browser *b);                 // serialize + push
int  undo_reopen_top(UndoStack *u, TabManager *tm);                  // returns new tab id or -1

//  command if enabled
void autosave_on (TabManager *tm, const char *path_or_null);
void autosave_off(TabManager *tm);
void autosave_maybe(const TabManager *tm);

#endif

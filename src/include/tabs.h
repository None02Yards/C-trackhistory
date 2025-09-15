#ifndef TABS_H
#define TABS_H


#include "browser.h"
#include "bookmarks.h"   


typedef struct {
    Browser **tabs;
    int count, cap;
    int active;
    int back_cap_default;

    // NEW:
    Vec  closed_json;     
    int  autosave;         // 0/1
    char autosave_path[260];
    BMList bookmarks;      
} TabManager;

void tm_init(TabManager *tm, int back_cap_default);
int tm_new_tab(TabManager *tm, const char *homepage);
void tm_close_tab(TabManager *tm, int id);
void tm_switch(TabManager *tm, int id);
Browser *tm_active(TabManager *tm);
void tm_destroy(TabManager *tm);



#endif 
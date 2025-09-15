#ifndef SESSION_H
#define SESSION_H


#include "tabs.h"


int save_session_json(const char *path, const TabManager *tm);
int load_session_json(const char *path, TabManager *tm, int back_cap_default);

// NEW: serialize a single Browser (JSON object) to a malloc'd string
char *session_serialize_tab_json(const Browser *b);
// NEW: parse a single Browser JSON object blob into a Browser*
int session_deserialize_tab_json(const char *obj_json, Browser **out, int back_cap_default);

#endif // SESSION_H
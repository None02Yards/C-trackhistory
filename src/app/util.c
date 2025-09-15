#include <stdlib.h>
#include <string.h>
#include "util.h"


char *sdup(const char *s) {
if (!s) return NULL;
size_t n = strlen(s) + 1;
char *p = (char*)malloc(n);
if (p) memcpy(p, s, n);
return p;
}
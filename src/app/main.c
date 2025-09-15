// src/app/main.c — interactive + batch (file/pipe) CLI

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "tabs.h"
#include "browser.h"
#include "session.h"
#include "ring.h"       // for ring_at in print
#include "features.h"   // undo + autosave
#include "bookmarks.h"  // bookmarks commands

#if defined(_WIN32) || defined(_WIN64)
  #include <io.h>          // _isatty, _fileno
  #define isatty  _isatty
  #define fileno  _fileno
#else
  #include <unistd.h>      // isatty, fileno
#endif

/* ------------------ helpers ------------------ */
static void print_tab(const Browser *b) {
    printf("CURRENT: %s\n", b->current);
    printf("BACK   : [");
    for (int j=0; j<b->back.size; ++j) {
        if (j) printf(", ");
        printf("%s", ring_at(&b->back, j));
    }
    printf("]\nFORWARD: [");
    for (int j=0; j<b->fwd.size; ++j) {
        if (j) printf(", ");
        printf("%s", b->fwd.data[j]);
    }
    printf("]\n");
}

static char* lstrip(char *s){ while(isspace((unsigned char)*s)) s++; return s; }
static void  rstrip(char *s){ size_t n=strlen(s); while(n&&isspace((unsigned char)s[n-1])) s[--n]='\0'; }

static void print_help(void) {
    puts(
"commands:\n"
"  visit <url>\n"
"  back [n]\n"
"  forward [n]\n"
"  current\n"
"  print\n"
"  tabs\n"
"  newtab [homepage]\n"
"  switch <id>\n"
"  close [id]\n"
"  reopen\n"
"  autosave [on|off|<path.json>]\n"
"  bm add <name> <url>\n"
"  bm list\n"
"  bm open <id|name>\n"
"  save <path.json>\n"
"  load <path.json>\n"
"  quit"
    );
}

/* returns 1 to continue loop, 0 to exit */
static int process_command(TabManager *tm, UndoStack *undo, const char *cmdline) {
    char buf[2048];
    strncpy(buf, cmdline, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    char *line = lstrip(buf);
    // allow lines copied with a leading prompt '>'
    if (*line == '>') { line++; while (*line==' '||*line=='\t') line++; }
    if (*line == '#') return 1;   // comment
    rstrip(line);
    if (*line == '\0') return 1;  // empty

    // first token (lowercased)
    char *sp = strpbrk(line, " \t");
    size_t clen = sp ? (size_t)(sp - line) : strlen(line);
    char cbuf[64]; if (clen >= sizeof cbuf) clen = sizeof cbuf - 1;
    memcpy(cbuf, line, clen); cbuf[clen] = '\0';
    for (char *p=cbuf; *p; ++p) *p = (char)tolower((unsigned char)*p);

    char *arg = sp ? lstrip(sp) : NULL;
    Browser *b = tm_active(tm);

    if (strcmp(cbuf, "help") == 0) {
        print_help();

    } else if (strcmp(cbuf, "visit") == 0) {
        if (!b) { puts("no active tab"); return 1; }
        if (!arg || !*arg) { puts("usage: visit <url>"); return 1; }
        browser_visit(b, arg);
        puts(b->current);
        autosave_maybe(tm);

    } else if (strcmp(cbuf, "back") == 0) {
        if (!b) { puts("no active tab"); return 1; }
        int n = 1; if (arg && *arg) n = atoi(arg);
        printf("%s\n", browser_back(b, n));
        autosave_maybe(tm);

    } else if (strcmp(cbuf, "forward") == 0) {
        if (!b) { puts("no active tab"); return 1; }
        int n = 1; if (arg && *arg) n = atoi(arg);
        printf("%s\n", browser_forward(b, n));
        autosave_maybe(tm);

    } else if (strcmp(cbuf, "current") == 0) {
        if (!b) { puts("no active tab"); return 1; }
        puts(browser_current(b));

    } else if (strcmp(cbuf, "print") == 0) {
        if (!b) { puts("no active tab"); return 1; }
        print_tab(b);

    } else if (strcmp(cbuf, "tabs") == 0) {
        if (tm->count == 0) { puts("(no tabs)"); return 1; }
        for (int i=0;i<tm->count;++i) {
            printf("[%d]%s %s\n", i, (i==tm->active)?"*":" ", tm->tabs[i]->current);
        }

    } else if (strcmp(cbuf, "newtab") == 0) {
        const char *home = (arg && *arg) ? arg : "about:blank";
        int id = tm_new_tab(tm, home);
        tm_switch(tm, id);
        printf("opened tab %d -> %s\n", id, tm->tabs[id]->current);
        autosave_maybe(tm);

    } else if (strcmp(cbuf, "switch") == 0) {
        if (!arg || !*arg) { puts("usage: switch <id>"); return 1; }
        int id = atoi(arg);
        if (0 <= id && id < tm->count) {
            tm_switch(tm, id);
            printf("active tab %d -> %s\n", id, tm->tabs[id]->current);
            autosave_maybe(tm);
        } else {
            puts("invalid tab id");
        }

    } else if (strcmp(cbuf, "close") == 0) {
        int id = (arg && *arg) ? atoi(arg) : tm->active;
        if (tm->count == 0) { puts("no tabs"); return 1; }
        if (!(0 <= id && id < tm->count)) { puts("invalid tab id"); return 1; }
        // push closed tab to undo stack, then close
        undo_push_tab(undo, tm->tabs[id]);
        tm_close_tab(tm, id);
        if (tm->count) printf("now at tab %d -> %s\n", tm->active, tm->tabs[tm->active]->current);
        else puts("(all tabs closed)");
        autosave_maybe(tm);

    } else if (strcmp(cbuf, "reopen") == 0) {
        int id = undo_reopen_top(undo, tm);
        if (id >= 0) { printf("reopened tab %d -> %s\n", id, tm->tabs[id]->current); autosave_maybe(tm); }
        else puts("nothing to reopen");

    } else if (strcmp(cbuf, "autosave") == 0) {
        if (!arg || !*arg) {
            printf("autosave is %s (%s)\n", tm->autosave?"on":"off",
                   tm->autosave_path[0]?tm->autosave_path:"session.json");
        } else if (strncmp(arg,"on",2)==0) {
            autosave_on(tm, NULL); puts("autosave on");
        } else if (strncmp(arg,"off",3)==0) {
            autosave_off(tm); puts("autosave off");
        } else { // treat as path
            autosave_on(tm, arg); printf("autosave on (%s)\n", tm->autosave_path);
        }

    } else if (strcmp(cbuf, "bm") == 0) {
        if (!arg||!*arg) { puts("usage: bm add <name> <url> | bm list | bm open <id|name>"); }
        else {
            char sub[16]; char rest[1024]; sub[0]=0; rest[0]=0;
            sscanf(arg, "%15s %[^\n]", sub, rest);
            for (char *p=sub;*p;++p)*p=(char)tolower((unsigned char)*p);

            if (strcmp(sub,"add")==0) {
                char name[256], url[768];
                if (sscanf(rest, "%255s %767s", name, url) != 2){ puts("usage: bm add <name> <url>"); }
                else { int idx=bm_add(&tm->bookmarks, name, url); printf("bookmark [%d] %s -> %s\n", idx, name, url); autosave_maybe(tm); }
            } else if (strcmp(sub,"list")==0) {
                if (tm->bookmarks.size==0) puts("(no bookmarks)");
                for (int i=0;i<tm->bookmarks.size;++i){
                    printf("[%d] %s -> %s\n", i, tm->bookmarks.data[i].name, tm->bookmarks.data[i].url);
                }
            } else if (strcmp(sub,"open")==0) {
                if (!b){ puts("no active tab"); }
                else if (!*rest){ puts("usage: bm open <id|name>"); }
                else {
                    int id=-1;
                    if (isdigit((unsigned char)rest[0])) id = atoi(rest);
                    else id = bm_find_by_name(&tm->bookmarks, rest);
                    const BMItem* it = bm_get(&tm->bookmarks, id);
                    if (!it) puts("bookmark not found");
                    else { browser_visit(b, it->url); puts(b->current); autosave_maybe(tm); }
                }
            } else {
                puts("usage: bm add <name> <url> | bm list | bm open <id|name>");
            }
        }

    } else if (strcmp(cbuf, "save") == 0) {
        if (!arg || !*arg) { puts("usage: save <file.json>"); return 1; }
        if (save_session_json(arg, tm)) puts("saved.");
        else puts("save failed.");

    } else if (strcmp(cbuf, "load") == 0) {
        if (!arg || !*arg) { puts("usage: load <file.json>"); return 1; }
        if (load_session_json(arg, tm, tm->back_cap_default)) { puts("loaded."); autosave_maybe(tm); }
        else puts("load failed.");

    } else if (strcmp(cbuf, "quit") == 0 || strcmp(cbuf, "exit") == 0) {
        return 0; // stop loop
    } else {
        puts("unknown command. try 'help'.");
    }
    return 1;
}

static void usage(const char *prog) {
    printf("usage:\n"
           "  %s                # interactive (stdin)\n"
           "  %s -f file.txt    # batch from file\n"
           "  %s file.txt       # batch from file (shorthand)\n"
           "  %s < file.txt     # batch from stdin redirection\n", prog, prog, prog, prog);
}

/* ------------------ main ------------------ */
int main(int argc, char **argv) {
    const int BACK_CAP = 5;

    TabManager tm; tm_init(&tm, BACK_CAP);
    UndoStack  undo; undo_init(&undo);

    tm_new_tab(&tm, "about:blank");

    FILE *in = NULL;
    int interactive = 1;

    if (argc == 1) {
        in = stdin; interactive = isatty(fileno(stdin)); // prompt only if TTY
    } else if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            usage(argv[0]); tm_destroy(&tm); undo_destroy(&undo); return 0;
        }
        in = fopen(argv[1], "rb");
        if (!in) { perror("open"); tm_destroy(&tm); undo_destroy(&undo); return 1; }
        interactive = 0;
    } else if (argc == 3 && (strcmp(argv[1], "-f") == 0 || strcmp(argv[1], "--file") == 0)) {
        in = fopen(argv[2], "rb");
        if (!in) { perror("open"); tm_destroy(&tm); undo_destroy(&undo); return 1; }
        interactive = 0;
    } else {
        usage(argv[0]); tm_destroy(&tm); undo_destroy(&undo); return 1;
    }

    if (interactive) { puts("browser ready. type 'help' for commands."); print_help(); }

    char line[4096];
    for (;;) {
        if (interactive) printf("> ");
        if (!fgets(line, sizeof line, in)) break;
        if (!process_command(&tm, &undo, line)) break;
    }

    if (in && in != stdin) fclose(in);
    tm_destroy(&tm);
    undo_destroy(&undo);
    return 0;
}

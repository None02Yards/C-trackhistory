#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "session.h"
#include "bookmarks.h"
#include "util.h"

/* forward declaration for internal helper */
static void json_escape_str_mem(char **pbuf, size_t *plen, size_t *pcap, const char *s);


/*  JSON writer  */
static void json_escape_str(FILE *f, const char *s) {
    fputc('"', f);
    for (const unsigned char *p=(const unsigned char*)s; *p; ++p) {
        unsigned char c = *p;
        switch (c) {
            case '\\': fputs("\\\\", f); break;
            case '"': fputs("\\\"", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (c < 0x20) fprintf(f, "\\u%04x", c);
                else fputc((int)c, f);
        }
    }
    fputc('"', f);
}

int save_session_json(const char *path, const TabManager *tm) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    fputs("{\"tabs\":[", f);
    for (int i = 0; i < tm->count; ++i) {
        const Browser *b = tm->tabs[i];
        if (i) fputc(',', f);
        fputc('{', f);

        fputs("\"current\":", f);
        json_escape_str(f, b->current);

        fputs(",\"back\":[", f);
        for (int j = 0; j < b->back.size; ++j) {
            if (j) fputc(',', f);
            json_escape_str(f, ring_at(&b->back, j));
        }
        fputc(']', f);

        fputs(",\"forward\":[", f);
        for (int j = 0; j < b->fwd.size; ++j) {
            if (j) fputc(',', f);
            json_escape_str(f, b->fwd.data[j]);
        }
        fputc(']', f);

        fputc('}', f);
    }
    fprintf(f, "],\"active\":%d}\n", tm->active < 0 ? 0 : tm->active);
    fclose(f);
    return 1;
}

/*  Minimal JSON reader  */

typedef struct { const char *s; size_t i, n; } JIn;
static void jin_init(JIn *in, const char *buf, size_t n) { in->s = buf; in->i = 0; in->n = n; }
static void jin_skip_ws(JIn *in) { while (in->i < in->n && isspace((unsigned char)in->s[in->i])) in->i++; }
static int  jin_expect(JIn *in, char c) { jin_skip_ws(in); if (in->i < in->n && in->s[in->i]==c) { in->i++; return 1; } return 0; }
static char *jin_read_string(JIn *in) {
    jin_skip_ws(in);
    if (in->i >= in->n || in->s[in->i] != '"') return NULL;
    in->i++;
    char *out = NULL; size_t cap = 0, len = 0;
    while (in->i < in->n) {
        unsigned char c = (unsigned char)in->s[in->i++];
        if (c == '"') break;
        if (c == '\\') {
            if (in->i >= in->n) break;
            unsigned char e = (unsigned char)in->s[in->i++];
            switch (e) {
                case 'n': c='\n'; break; case 'r': c='\r'; break; case 't': c='\t'; break;
                case '\\': c='\\'; break; case '"': c='"'; break;
                case 'u': if (in->i + 4 <= in->n) in->i += 4; c='?'; break;
                default: c=e; break;
            }
        }
        if (len + 1 >= cap) { cap = cap ? cap * 2 : 32; out = (char*)realloc(out, cap); }
        out[len++] = (char)c;
    }
    if (!out) return sdup("");
    out[len] = '\0';
    return out;
}

static int jin_read_string_array(JIn *in, Vec *v) {
    v->size = 0;
    if (!jin_expect(in, '[')) return 0;
    jin_skip_ws(in);
    if (jin_expect(in, ']')) return 1; // empty
    do {
        char *s = jin_read_string(in); if (!s) return 0;
        vec_push(v, s);
        jin_skip_ws(in);
    } while (jin_expect(in, ','));
    return jin_expect(in, ']');
}

static int jin_read_tab(JIn *in, Browser **out, int back_cap) {
    if (!jin_expect(in, '{')) return 0;

    char *cur = NULL; Vec backV; vec_init(&backV); Vec fwdV; vec_init(&fwdV);

    for (;;) {
        jin_skip_ws(in);
        if (jin_expect(in, '}')) break;
        char *key = jin_read_string(in); if (!key) { vec_free(&backV); vec_free(&fwdV); return 0; }
        if (!jin_expect(in, ':')) { free(key); vec_free(&backV); vec_free(&fwdV); return 0; }
        if (strcmp(key, "current") == 0) {
            free(cur); cur = jin_read_string(in); if (!cur) { free(key); vec_free(&backV); vec_free(&fwdV); return 0; }
        } else if (strcmp(key, "back") == 0) {
            vec_clear_free(&backV);
            if (!jin_read_string_array(in, &backV)) { free(key); free(cur); vec_free(&backV); vec_free(&fwdV); return 0; }
        } else if (strcmp(key, "forward") == 0) {
            vec_clear_free(&fwdV);
            if (!jin_read_string_array(in, &fwdV)) { free(key); free(cur); vec_free(&backV); vec_free(&fwdV); return 0; }
        } else { free(key); free(cur); vec_free(&backV); vec_free(&fwdV); return 0; }
        free(key);
        jin_skip_ws(in); if (jin_expect(in, ',')) continue; if (jin_expect(in, '}')) break;
    }

    Browser *b = (Browser*)malloc(sizeof(Browser));
    browser_init(b, cur ? cur : "", back_cap);

    for (int i = 0; i < backV.size; ++i) ring_push_back(&b->back, backV.data[i]);
    backV.size = 0; vec_free(&backV);

    free(b->current); b->current = cur ? cur : sdup("");

    for (int i = 0; i < fwdV.size; ++i) vec_push(&b->fwd, fwdV.data[i]);
    fwdV.size = 0; vec_free(&fwdV);

    *out = b; return 1;
}

int load_session_json(const char *path, TabManager *tm, int back_cap_default) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    char *buf = (char*)malloc((size_t)n + 1); if (!buf) { fclose(f); return 0; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);

    JIn in; jin_init(&in, buf, (size_t)n);
    if (!jin_expect(&in, '{')) { free(buf); return 0; }

    TabManager tmp; tm_init(&tmp, back_cap_default);
    int read_tabs = 0, read_active = 0;

    for (;;) {
        jin_skip_ws(&in);
        if (jin_expect(&in, '}')) break;
        char *key = jin_read_string(&in); if (!key) { free(buf); tm_destroy(&tmp); return 0; }
        if (!jin_expect(&in, ':')) { free(key); free(buf); tm_destroy(&tmp); return 0; }
        if (strcmp(key, "tabs") == 0) {
            if (!jin_expect(&in, '[')) { free(key); free(buf); tm_destroy(&tmp); return 0; }
            jin_skip_ws(&in);
            if (!jin_expect(&in, ']')) {
                do {
                    Browser *b = NULL; if (!jin_read_tab(&in, &b, back_cap_default)) { free(key); free(buf); tm_destroy(&tmp); return 0; }
                    // append
                    // reserve
                    if (tmp.cap <= tmp.count) {
                        int c = tmp.cap ? tmp.cap : 4; while (c <= tmp.count) c <<= 1;
                        tmp.tabs = (Browser**)realloc(tmp.tabs, (size_t)c * sizeof(Browser*)); tmp.cap = c;
                    }
                    tmp.tabs[tmp.count++] = b;
                    jin_skip_ws(&in);
                } while (jin_expect(&in, ','));
                if (!jin_expect(&in, ']')) { free(key); free(buf); tm_destroy(&tmp); return 0; }
            }
            read_tabs = 1;
        } else if (strcmp(key, "active") == 0) {
            jin_skip_ws(&in);
            int sign = 1; if (in.s[in.i]=='-') { sign=-1; in.i++; }
            int val = 0, any = 0; while (in.i < in.n && isdigit((unsigned char)in.s[in.i])) { val = val*10 + (in.s[in.i]-'0'); in.i++; any=1; }
            if (!any) { free(key); free(buf); tm_destroy(&tmp); return 0; }
            tmp.active = sign*val; read_active = 1;
        } else { free(key); free(buf); tm_destroy(&tmp); return 0; }
        free(key);
        jin_skip_ws(&in); if (jin_expect(&in, ',')) continue; if (jin_expect(&in, '}')) break;
    }
    free(buf);

    if (!read_tabs) { tm_destroy(&tmp); return 0; }
    if (!read_active || tmp.active < 0 || tmp.active >= tmp.count) tmp.active = (tmp.count ? 0 : -1);

    tm_destroy(tm); *tm = tmp; return 1;
}

char *session_serialize_tab_json(const Browser *b){
    // write to a growing memory buffer
    size_t cap = 1024, len = 0;
    char *buf = (char*)malloc(cap);
    #define EMIT(...) do{ \
        char tmp[1024]; int n = snprintf(tmp, sizeof tmp, __VA_ARGS__); \
        if (len + n + 1 > cap){ while(len+n+1>cap) cap<<=1; buf=(char*)realloc(buf,cap);} \
        memcpy(buf+len, tmp, (size_t)n); len+=n; buf[len]='\0'; \
    }while(0)

    EMIT("{\"current\":"); json_escape_str_mem(&buf,&len,&cap,b->current);
    EMIT(",\"back\":[");
    for (int j=0;j<b->back.size;++j){
        if (j) EMIT(",");
        json_escape_str_mem(&buf,&len,&cap, ring_at(&b->back,j));
    }
    EMIT("],\"forward\":[");
    for (int j=0;j<b->fwd.size;++j){
        if (j) EMIT(",");
        json_escape_str_mem(&buf,&len,&cap, b->fwd.data[j]);
    }
    EMIT("]}");
    #undef EMIT
    return buf;
}

void json_escape_str_mem(char **pbuf, size_t *plen, size_t *pcap, const char *s){
    // ensure capacity helper
    #define ENS(N) do{ if(*plen + (N) + 1 > *pcap){ while(*plen+(N)+1>*pcap) *pcap<<=1; *pbuf=(char*)realloc(*pbuf,*pcap);} }while(0)
    ENS(1); (*pbuf)[(*plen)++]='"'; (*pbuf)[*plen]='\0';
    for (const unsigned char *p=(const unsigned char*)s; *p; ++p){
        unsigned char c=*p;
        switch(c){
            case '\\': ENS(2); (*pbuf)[(*plen)++]='\\'; (*pbuf)[(*plen)++]='\\'; break;
            case '"':  ENS(2); (*pbuf)[(*plen)++]='\\'; (*pbuf)[(*plen)++]='"';  break;
            case '\n': ENS(2); (*pbuf)[(*plen)++]='\\'; (*pbuf)[(*plen)++]='n';  break;
            case '\r': ENS(2); (*pbuf)[(*plen)++]='\\'; (*pbuf)[(*plen)++]='r';  break;
            case '\t': ENS(2); (*pbuf)[(*plen)++]='\\'; (*pbuf)[(*plen)++]='t';  break;
            default:
                if (c<0x20){ ENS(6); int n=sprintf(*pbuf+*plen,"\\u%04x",c); *plen+= (size_t)n; }
                else { ENS(1); (*pbuf)[(*plen)++]= (char)c; }
        }
        (*pbuf)[*plen]='\0';
    }
    ENS(1); (*pbuf)[(*plen)++]='"'; (*pbuf)[*plen]='\0';
    #undef ENS
}

int session_deserialize_tab_json(const char *obj_json, Browser **out, int back_cap_default){
    // Wrap in a tiny session to reuse existing jin_read_tab() machinery
    size_t L = strlen(obj_json);
    char *doc = (char*)malloc(L + 32);
    sprintf(doc, "{ \"tabs\":[%s],\"active\":0 }", obj_json);

    TabManager tmp; tm_init(&tmp, back_cap_default);
    int ok = 0;
    // reuse load_session_json on memory: if your loader only supports files,
    // write to a temp file, then load. If it supports buffer, call directly.
    // Here, for simplicity, temp file approach:
    char path[] = ".__reopen_tmp.json";
    FILE *f = fopen(path,"wb"); if (!f){ free(doc); tm_destroy(&tmp); return 0; }
    fwrite(doc,1,strlen(doc),f); fclose(f);
    ok = load_session_json(path, &tmp, back_cap_default);
    remove(path);
    free(doc);

    if (!ok || tmp.count == 0){ tm_destroy(&tmp); return 0; }
    *out = tmp.tabs[0];
    // move ownership of first tab out; clean the rest of tmp
    for (int i=1;i<tmp.count;++i){ browser_destroy(tmp.tabs[i]); free(tmp.tabs[i]); }
    free(tmp.tabs);
    tmp.tabs=NULL; tmp.count=0; tmp.cap=0;
    tm_destroy(&tmp);
    return 1;
}
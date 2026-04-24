#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include "../include/util/html2text.h"

/* Return 0 on success, -1 jika realloc gagal (caller harus abort). */
static int append(char** buf, size_t* len, size_t* cap, const char* s) {
    size_t slen = strlen(s);
    if (*len + slen + 1 > *cap) {
        size_t newcap = (*len + slen + 1) * 2;
        char* nbuf = (char*)realloc(*buf, newcap);
        if (!nbuf) return -1;   /* leave *buf intact untuk caller bersihkan */
        *buf = nbuf;
        *cap = newcap;
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = '\0';
    return 0;
}

static int tag_matches(const char* tag, size_t tlen, const char* name) {
    size_t nlen = strlen(name);
    if (tlen < nlen) return 0;
    return strncasecmp(tag, name, nlen) == 0 &&
           (tlen == nlen || tag[nlen] == ' ' || tag[nlen] == '/' || tag[nlen] == '>');
}

char* html_to_text(const char* html) {
    if (!html) { char* s = (char*)malloc(1); if (s) s[0] = '\0'; return s; }
    size_t cap = strlen(html) + 32;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t len = 0;
    out[0] = '\0';
    #define APP(str) do { if (append(&out, &len, &cap, (str)) < 0) { free(out); return NULL; } } while (0)

    const char* p = html;
    while (*p) {
        if (*p == '<') {
            const char* end = strchr(p, '>');
            if (!end) { APP(p); break; }
            size_t tlen = (size_t)(end - p - 1);
            const char* tag = p + 1;
            int is_closing = (*tag == '/');
            if (is_closing) { tag++; tlen--; }
            /* Hanya opening tag yang memancarkan marker. Closing tag di-strip saja
             * supaya input seperti <li>Item</li> tidak menghasilkan bullet kosong. */
            if (!is_closing) {
                if (tag_matches(tag, tlen, "br"))        APP("\n");
                else if (tag_matches(tag, tlen, "li"))   APP("\n- ");
                else if (tag_matches(tag, tlen, "p"))    APP("\n");
                else if (tag_matches(tag, tlen, "ul"))   APP("\n");
                else if (tag_matches(tag, tlen, "ol"))   APP("\n");
                else if (tag_matches(tag, tlen, "div"))  APP("\n");
            }
            /* tag lain (b, i, span, a, strong, em) dibiarkan — strip tag saja */
            p = end + 1;
            continue;
        }
        if (*p == '&') {
            if (strncmp(p, "&amp;", 5) == 0)       { APP("&");  p += 5; continue; }
            if (strncmp(p, "&lt;", 4) == 0)        { APP("<");  p += 4; continue; }
            if (strncmp(p, "&gt;", 4) == 0)        { APP(">");  p += 4; continue; }
            if (strncmp(p, "&quot;", 6) == 0)      { APP("\""); p += 6; continue; }
            if (strncmp(p, "&#39;", 5) == 0)       { APP("'");  p += 5; continue; }
            if (strncmp(p, "&nbsp;", 6) == 0)      { APP(" ");  p += 6; continue; }
        }
        char s[2] = { *p, 0 };
        APP(s);
        p++;
    }
    #undef APP
    return out;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include "../include/util/html2text.h"

/* Ganti entitas HTML umum. Caller tanggung jawab untuk memastikan input valid. */
static void append(char** buf, size_t* len, size_t* cap, const char* s) {
    size_t slen = strlen(s);
    if (*len + slen + 1 > *cap) {
        *cap = (*len + slen + 1) * 2;
        *buf = (char*)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = '\0';
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
    size_t len = 0;
    if (!out) return NULL;
    out[0] = '\0';

    const char* p = html;
    while (*p) {
        if (*p == '<') {
            const char* end = strchr(p, '>');
            if (!end) { append(&out, &len, &cap, p); break; }
            size_t tlen = (size_t)(end - p - 1);
            const char* tag = p + 1;
            if (*tag == '/') { tag++; tlen--; }
            if (tag_matches(tag, tlen, "br"))        append(&out, &len, &cap, "\n");
            else if (tag_matches(tag, tlen, "li"))   append(&out, &len, &cap, "\n- ");
            else if (tag_matches(tag, tlen, "p"))    append(&out, &len, &cap, "\n");
            else if (tag_matches(tag, tlen, "ul"))   append(&out, &len, &cap, "\n");
            else if (tag_matches(tag, tlen, "ol"))   append(&out, &len, &cap, "\n");
            else if (tag_matches(tag, tlen, "div"))  append(&out, &len, &cap, "\n");
            /* tag lain (b, i, span, a, strong, em) dibiarkan — strip tag saja */
            p = end + 1;
            continue;
        }
        if (*p == '&') {
            if (strncmp(p, "&amp;", 5) == 0)       { append(&out, &len, &cap, "&");  p += 5; continue; }
            if (strncmp(p, "&lt;", 4) == 0)        { append(&out, &len, &cap, "<");  p += 4; continue; }
            if (strncmp(p, "&gt;", 4) == 0)        { append(&out, &len, &cap, ">");  p += 4; continue; }
            if (strncmp(p, "&quot;", 6) == 0)      { append(&out, &len, &cap, "\""); p += 6; continue; }
            if (strncmp(p, "&#39;", 5) == 0)       { append(&out, &len, &cap, "'");  p += 5; continue; }
            if (strncmp(p, "&nbsp;", 6) == 0)      { append(&out, &len, &cap, " ");  p += 6; continue; }
        }
        char s[2] = { *p, 0 };
        append(&out, &len, &cap, s);
        p++;
    }
    return out;
}

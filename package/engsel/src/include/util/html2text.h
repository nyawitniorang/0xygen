#ifndef ENGSEL_UTIL_HTML2TEXT_H
#define ENGSEL_UTIL_HTML2TEXT_H

#include <stddef.h>

/* Sederhanakan HTML ringan (<li>, <br>, <p>, <ul>, <ol>, &amp; dll) jadi
 * plain text. Kompatibel dengan deskripsi paket Python HTMLToText.
 * Return heap-allocated string (caller free()). */
char* html_to_text(const char* html);

#endif

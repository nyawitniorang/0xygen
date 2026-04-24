#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/util/phone.h"

char* normalize_msisdn(const char* input) {
    if (!input) return NULL;
    char cleaned[32];
    int j = 0;
    for (int i = 0; input[i] && j < 31; i++) {
        if (input[i] >= '0' && input[i] <= '9') cleaned[j++] = input[i];
    }
    cleaned[j] = '\0';
    int len = (int)strlen(cleaned);
    if (len < 9 || len > 14) return NULL;

    char* result = malloc(32);
    if (!result) return NULL;

    if (strncmp(cleaned, "62", 2) == 0) {
        snprintf(result, 32, "%s", cleaned);
    } else if (cleaned[0] == '0') {
        snprintf(result, 32, "62%s", cleaned + 1);
    } else if (cleaned[0] == '8') {
        snprintf(result, 32, "62%s", cleaned);
    } else {
        free(result);
        return NULL;
    }

    /* Sanity check: setelah prefix 62, total harus 10..14 digit */
    int rl = (int)strlen(result);
    if (rl < 10 || rl > 14) { free(result); return NULL; }
    return result;
}

#ifndef ENGSEL_UTIL_PHONE_H
#define ENGSEL_UTIL_PHONE_H

/* Normalisasi input MSISDN Indonesia:
 *   "08xxxx", "8xxxx", "628xxxx", "+628xxxx" semua jadi "628xxxx"
 * Return malloc'd string (caller harus free) atau NULL kalau panjang di luar
 * 10..14 digit atau tidak cocok pattern. */
char* normalize_msisdn(const char* input);

#endif

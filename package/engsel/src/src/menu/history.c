#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cJSON.h"
#include "../include/menu/history.h"
#include "../include/client/engsel.h"
#include "../include/util/json_util.h"
#include "../include/util/phone.h"
#include "../include/util/nav.h"

#define W 55

static void rule(char ch) { for (int i = 0; i < W; i++) putchar(ch); putchar('\n'); }

static void flush_line(void) { int c; while ((c = getchar()) != '\n' && c != EOF); }

static void pause_enter(void) {
    printf("\nTekan Enter untuk melanjutkan...");
    fflush(stdout); flush_line();
}

static void read_line(const char* prompt, char* buf, size_t n) {
    printf("%s", prompt); fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
}

static void read_msisdn(const char* prompt, char* buf, size_t n) {
    read_line(prompt, buf, n);
    if (!buf[0]) return;
    char* norm = normalize_msisdn(buf);
    if (norm) {
        snprintf(buf, n, "%s", norm);
        free(norm);
    }
}

static void clear_screen(void) { printf("\033[H\033[J"); fflush(stdout); }

static void section_header(const char* title) {
    clear_screen();
    rule('=');
    printf("  %s\n", title);
    rule('=');
}

/* Format timestamp Unix (detik) → "05 October 2025 | 11:10 WIB".
   Python dikurangi 7 jam — itu karena server menaruh timestamp dengan offset
   yang membuat localtime() tampil jam 18 untuk transaksi jam 11 WIB. Kita
   replikasi perilaku yang sama (TZ=UTC - 7 untuk approximate WIB). */
static void format_wib(double ts, char* out, size_t n) {
    if (ts <= 0) { snprintf(out, n, "--"); return; }
    time_t t = (time_t)ts - 7 * 3600;
    struct tm gm;
    gmtime_r(&t, &gm);
    /* "%d %B %Y | %H:%M WIB" */
    strftime(out, n, "%d %B %Y | %H:%M WIB", &gm);
}

void show_transaction_history_menu(const char* base_api, const char* api_key,
                                   const char* xdata_key, const char* x_api_secret,
                                   const char* id_token) {
    while (1) {
        section_header("Riwayat Transaksi");

        cJSON* res = get_transaction_history(base_api, api_key, xdata_key, x_api_secret, id_token);
        if (!json_status_is_success(res)) {
            printf("[-] Gagal ambil riwayat.\n");
            if (res) { char* out = cJSON_Print(res); if (out) { printf("%s\n", out); free(out); } }
            cJSON_Delete(res);
            pause_enter();
            return;
        }
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
        cJSON* list = data ? cJSON_GetObjectItemCaseSensitive(data, "list") : NULL;
        int n = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;

        if (n == 0) {
            printf("Tidak ada riwayat transaksi.\n");
        }
        for (int i = 0; i < n; i++) {
            cJSON* t = cJSON_GetArrayItem(list, i);
            const char* title = json_get_str(t, "title", "N/A");
            const char* price = json_get_str(t, "price", "N/A");
            const char* method = json_get_str(t, "payment_method_label", "N/A");
            const char* status = json_get_str(t, "status", "N/A");
            const char* pstatus = json_get_str(t, "payment_status", "N/A");
            double ts = json_get_double(t, "timestamp", 0);
            char when[64];
            format_wib(ts, when, sizeof(when));

            printf("%d. %s - %s\n", i + 1, title, price);
            printf("   Tanggal: %s\n", when);
            printf("   Metode Pembayaran: %s\n", method);
            printf("   Status Transaksi: %s\n", status);
            printf("   Status Pembayaran: %s\n", pstatus);
            rule('-');
        }
        cJSON_Delete(res);

        printf("0.  Refresh\n"
               "00. Kembali\n"
               "99. Kembali ke menu utama\n"
               "Pilih opsi: "); fflush(stdout);
        char ch[16];
        if (!fgets(ch, sizeof(ch), stdin)) return;
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0) return;
        if (strcmp(ch, "99") == 0) { nav_trigger_goto_main(); return; }
        /* selain itu (termasuk "0") refresh otomatis via loop */
    }
}

/* Helper: ambil nilai status code dari response (root.status atau root.code).
 * Kalau response sukses, kembalikan NULL. */
static const char* response_error_code(cJSON* r) {
    if (!r) return NULL;
    /* Format umum response error: { "status": "FAILED", "code": "164",
     * "code_detail": "164", "message": "", ... }. Kalau status SUCCESS,
     * field code biasanya "00" / tidak ada. */
    cJSON* st = cJSON_GetObjectItemCaseSensitive(r, "status");
    if (cJSON_IsString(st) && st->valuestring &&
        strcmp(st->valuestring, "SUCCESS") == 0) {
        return NULL;
    }
    cJSON* c = cJSON_GetObjectItemCaseSensitive(r, "code");
    if (cJSON_IsString(c) && c->valuestring && c->valuestring[0]) {
        return c->valuestring;
    }
    return NULL;
}

void show_register_menu(const char* base_api, const char* api_key,
                        const char* xdata_key, const char* x_api_secret,
                        const char* id_token) {
    char msisdn[32], nik[32], kk[32];
    section_header("Registrasi Kartu (Dukcapil)");
    read_msisdn("MSISDN (08/62xxxx) atau 99=batal: ", msisdn, sizeof(msisdn));
    if (strcmp(msisdn, "99") == 0 || !msisdn[0]) return;

    /* Pre-cek status registrasi sebelum minta NIK/KK. Server XL aslinya juga
     * memanggil endpoint ini lebih dulu di app MyXL — kita pakai untuk memberi
     * info lebih jelas ke user (sudah terdaftar / butuh biometrik / belum). */
    section_header("Registrasi Kartu (Dukcapil)");
    printf("MSISDN: %s\n", msisdn);
    printf("[*] Memeriksa status registrasi...\n");
    fflush(stdout);

    cJSON* info = get_registration_info(base_api, api_key, xdata_key,
                                        x_api_secret, id_token, msisdn);
    int proceed = 1;
    if (json_status_is_success(info)) {
        cJSON* d = cJSON_GetObjectItemCaseSensitive(info, "data");
        const char* reg_status   = json_get_str(d, "registration_status", "");
        const char* disp_nik     = json_get_str(d, "display_nik",         "");
        const char* disp_kk      = json_get_str(d, "display_kk",          "");
        const char* name         = json_get_str(d, "name",                "");
        const char* reg_date     = json_get_str(d, "registration_date",   "");
        cJSON* eligible_bio_node = d ? cJSON_GetObjectItemCaseSensitive(d, "eligible_biometric") : NULL;
        int eligible_bio = cJSON_IsTrue(eligible_bio_node);

        printf("\n");
        rule('-');
        printf("  Status saat ini: %s\n", reg_status[0] ? reg_status : "UNKNOWN");
        if (name[0])     printf("  Nama          : %s\n", name);
        if (disp_nik[0]) printf("  NIK           : %s\n", disp_nik);
        if (disp_kk[0])  printf("  KK            : %s\n", disp_kk);
        if (reg_date[0]) printf("  Tanggal       : %s\n", reg_date);
        rule('-');

        /* Kalau eligible biometric: beritahu user bahwa flow biometrik tidak
         * didukung di engsel. Mereka tetap bisa coba flow Dukcapil standar. */
        if (eligible_bio) {
            printf("[!] Nomor ini eligible untuk registrasi via biometrik\n"
                   "    (foto selfie + liveness). Engsel hanya mendukung\n"
                   "    flow Dukcapil standar (NIK + KK).\n");
        }

        if (strcmp(reg_status, "REGISTERED") == 0) {
            printf("[i] Nomor ini sudah terdaftar. Tidak perlu register ulang.\n");
            char ans[8];
            read_line("Tetap submit Dukcapil register? (y/N): ", ans, sizeof(ans));
            if (ans[0] != 'y' && ans[0] != 'Y') proceed = 0;
        }
    } else {
        printf("[!] Gagal mengambil status registrasi (lanjut tetap dicoba).\n");
        if (info) {
            const char* code = response_error_code(info);
            if (code) printf("    Kode info: %s\n", code);
        }
    }
    if (info) cJSON_Delete(info);

    if (!proceed) { pause_enter(); return; }

    printf("\n");
    read_line("NIK (16 digit) atau 99=batal: ", nik, sizeof(nik));
    if (strcmp(nik, "99") == 0 || !nik[0]) return;
    read_line("KK  (16 digit) atau 99=batal: ", kk, sizeof(kk));
    if (strcmp(kk, "99") == 0 || !kk[0]) return;

    printf("\n[*] Mengirim registrasi...\n"); fflush(stdout);
    cJSON* r = dukcapil_register(base_api, api_key, xdata_key, x_api_secret,
                                 id_token, msisdn, kk, nik);

    printf("\n");
    rule('=');
    if (json_status_is_success(r)) {
        printf("  [+] Registrasi BERHASIL.\n");
        printf("      MSISDN %s sudah terdaftar atas nama Anda.\n", msisdn);
    } else {
        const char* code = response_error_code(r);
        printf("  [-] Registrasi GAGAL.\n");
        if (code) {
            printf("      Kode error: %s\n", code);
            printf("      Pesan     : %s\n", register_error_message(code));
        } else {
            printf("      Server tidak mengembalikan kode error.\n");
        }
    }
    rule('=');

    /* Untuk debugging, tetap cetak JSON mentah agar user bisa kirim ke
     * maintainer kalau ada error code yang belum di-mapping. */
    if (r) {
        printf("\n--- Response (raw JSON) ---\n");
        char* out = cJSON_Print(r);
        if (out) { printf("%s\n", out); free(out); }
        cJSON_Delete(r);
    } else {
        printf("\n[-] Tidak ada response dari server.\n");
    }
    pause_enter();
}

void show_validate_menu(const char* base_api, const char* api_key,
                        const char* xdata_key, const char* x_api_secret,
                        const char* id_token) {
    char msisdn[32];
    section_header("Validate MSISDN");
    read_msisdn("MSISDN (08/62xxxx) atau 99=batal: ", msisdn, sizeof(msisdn));
    if (strcmp(msisdn, "99") == 0 || !msisdn[0]) return;
    cJSON* r = validate_msisdn(base_api, api_key, xdata_key, x_api_secret,
                               id_token, msisdn);
    printf("\n--- Response ---\n");
    if (r) { char* out = cJSON_Print(r); if (out) { printf("%s\n", out); free(out); } cJSON_Delete(r); }
    else   { printf("[-] Tidak ada response.\n"); }
    pause_enter();
}

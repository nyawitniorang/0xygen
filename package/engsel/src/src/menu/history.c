#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cJSON.h"
#include "../include/menu/history.h"
#include "../include/client/engsel.h"
#include "../include/util/json_util.h"
#include "../include/util/phone.h"

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

        printf("0. Refresh\n99. Kembali\nPilih opsi: "); fflush(stdout);
        char ch[16];
        if (!fgets(ch, sizeof(ch), stdin)) return;
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "99") == 0 || strcmp(ch, "00") == 0) return;
        /* selain itu (termasuk "0") refresh otomatis via loop */
    }
}

void show_register_menu(const char* base_api, const char* api_key,
                        const char* xdata_key, const char* x_api_secret,
                        const char* id_token) {
    char msisdn[32], nik[32], kk[32];
    section_header("Registrasi Kartu (Dukcapil)");
    read_msisdn("MSISDN (08/62xxxx) atau 99=batal: ", msisdn, sizeof(msisdn));
    if (strcmp(msisdn, "99") == 0 || !msisdn[0]) return;
    read_line("NIK: ", nik, sizeof(nik));
    if (strcmp(nik, "99") == 0 || !nik[0]) return;
    read_line("KK : ", kk,  sizeof(kk));
    if (strcmp(kk, "99") == 0 || !kk[0]) return;

    cJSON* r = dukcapil_register(base_api, api_key, xdata_key, x_api_secret,
                                 id_token, msisdn, kk, nik);
    printf("\n--- Response ---\n");
    if (r) { char* out = cJSON_Print(r); if (out) { printf("%s\n", out); free(out); } cJSON_Delete(r); }
    else   { printf("[-] Tidak ada response.\n"); }
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

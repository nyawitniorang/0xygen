#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include "../include/cJSON.h"
#include "../include/menu/store_browse.h"
#include "../include/menu/purchase_flow.h"
#include "../include/client/store.h"
#include "../include/client/engsel.h"
#include "../include/util/json_util.h"

#define W 55
extern double active_number;
extern char active_subs_type[32];

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

static void lower_in_place(char* s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* Map kode input "a1","b2", dll → package_info lookup. Keyed di array flat. */
typedef struct {
    char  code[16];
    char  action_type[24];
    char  action_param[128];
} StoreItem;

/* ===== 1. Family List ===== */

static void clear_screen_local(void) { printf("\033[H\033[J"); fflush(stdout); }

void show_store_family_list_browse(const char* base, const char* key,
                                   const char* xdata, const char* sec,
                                   const char* id_token, const char* subs_type) {
    while (1) {
        clear_screen_local();
        rule('='); printf("  Store > Family List\n"); rule('=');
        printf("Fetching family list...\n");
        cJSON* res = store_get_family_list(base, key, xdata, sec, id_token,
                                           subs_type ? subs_type : "PREPAID", 0);
        if (!json_status_is_success(res)) {
            printf("[-] Gagal ambil family list.\n");
            if (res) { char* out = cJSON_Print(res); if (out) { printf("%s\n", out); free(out); } }
            cJSON_Delete(res); pause_enter(); return;
        }
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
        cJSON* list = data ? cJSON_GetObjectItemCaseSensitive(data, "results") : NULL;
        int n = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;

        clear_screen_local();
        rule('='); printf("  Store > Family List\n"); rule('=');
        for (int i = 0; i < n; i++) {
            cJSON* f = cJSON_GetArrayItem(list, i);
            printf("%d. %s\n   Family code: %s\n", i + 1,
                   json_get_str(f, "label", "N/A"),
                   json_get_str(f, "id", "N/A"));
            rule('-');
        }
        printf("00. Kembali\n99. Menu utama\n"
               "Pilih nomor family untuk browse & beli paket: ");
        fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) { cJSON_Delete(res); return; }
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0) { cJSON_Delete(res); return; }
        if (strcmp(ch, "99") == 0) { cJSON_Delete(res); return; }
        int sel = atoi(ch);
        if (sel > 0 && sel <= n) {
            cJSON* f = cJSON_GetArrayItem(list, sel - 1);
            const char* fc = json_get_str(f, "id", "");
            cJSON_Delete(res);
            if (fc && fc[0]) {
                int goto_main = purchase_flow_by_family_code(fc);
                if (goto_main) return;
            }
            continue;
        }
        cJSON_Delete(res);
    }
}

/* ===== 2. Store Packages (search v9) ===== */

void show_store_packages_browse(const char* base, const char* key,
                                const char* xdata, const char* sec,
                                const char* id_token, const char* subs_type) {
    while (1) {
        clear_screen_local();
        rule('='); printf("  Store > Packages (Search v9)\n"); rule('=');
        printf("Fetching store packages...\n");
        cJSON* res = store_get_packages(base, key, xdata, sec, id_token,
                                        subs_type ? subs_type : "PREPAID", 0);
        if (!json_status_is_success(res)) {
            printf("[-] Gagal ambil store packages.\n");
            if (res) { char* out = cJSON_Print(res); if (out) { printf("%s\n", out); free(out); } }
            cJSON_Delete(res); pause_enter(); return;
        }
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
        cJSON* list = data ? cJSON_GetObjectItemCaseSensitive(data, "results_price_only") : NULL;
        int n = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
        clear_screen_local();
        rule('='); printf("  Store > Packages (Search v9)\n"); rule('=');

        StoreItem* items = (StoreItem*)calloc((size_t)n + 1, sizeof(StoreItem));
        if (!items) { printf("[-] OOM.\n"); cJSON_Delete(res); return; }
        for (int i = 0; i < n; i++) {
            cJSON* p = cJSON_GetArrayItem(list, i);
            int discounted = json_get_int(p, "discounted_price", 0);
            int original = json_get_int(p, "original_price", 0);
            int price = discounted > 0 ? discounted : original;
            snprintf(items[i].code, sizeof(items[i].code), "%d", i + 1);
            snprintf(items[i].action_type, sizeof(items[i].action_type), "%s",
                     json_get_str(p, "action_type", ""));
            snprintf(items[i].action_param, sizeof(items[i].action_param), "%s",
                     json_get_str(p, "action_param", ""));
            printf("%d. %s\n", i + 1, json_get_str(p, "title", "N/A"));
            printf("   Family: %s\n", json_get_str(p, "family_name", "N/A"));
            printf("   Price: Rp%d\n", price);
            printf("   Validity: %s\n", json_get_str(p, "validity", "N/A"));
            rule('-');
        }
        printf("00. Kembali\n99. Menu utama\nPilih nomor paket: "); fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) { free(items); cJSON_Delete(res); return; }
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0 || strcmp(ch, "99") == 0) { free(items); cJSON_Delete(res); return; }
        int sel = atoi(ch);
        if (sel > 0 && sel <= n) {
            const char* at = items[sel - 1].action_type;
            const char* ap = items[sel - 1].action_param;
            if (strcmp(at, "PDP") == 0 && ap[0]) {
                purchase_flow_by_option_code(ap);
            } else {
                printf("Action type %s belum didukung. Param: %s\n", at, ap);
                pause_enter();
            }
        }
        free(items);
        cJSON_Delete(res);
    }
}

/* ===== 3. Segments ===== */

void show_store_segments_browse(const char* base, const char* key,
                                const char* xdata, const char* sec,
                                const char* id_token) {
    while (1) {
        clear_screen_local();
        rule('='); printf("  Store > Segments\n"); rule('=');
        printf("Fetching store segments...\n");
        cJSON* res = store_get_segments(base, key, xdata, sec, id_token, 0);
        if (!json_status_is_success(res)) {
            printf("[-] Gagal ambil segments.\n");
            if (res) { char* out = cJSON_Print(res); if (out) { printf("%s\n", out); free(out); } }
            cJSON_Delete(res); pause_enter(); return;
        }
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
        cJSON* segs = data ? cJSON_GetObjectItemCaseSensitive(data, "store_segments") : NULL;
        int ns = cJSON_IsArray(segs) ? cJSON_GetArraySize(segs) : 0;

        clear_screen_local();
        rule('='); printf("  Store > Segments\n"); rule('=');
        /* Alokasi untuk max 26 x 32 = ~832 items */
        StoreItem* items = (StoreItem*)calloc(26 * 32, sizeof(StoreItem));
        if (!items) { printf("[-] OOM.\n"); cJSON_Delete(res); return; }
        int total = 0;

        for (int i = 0; i < ns && i < 26; i++) {
            cJSON* s = cJSON_GetArrayItem(segs, i);
            char letter = (char)('A' + i);
            rule('-');
            printf("%c. Banner: %s\n", letter, json_get_str(s, "title", "N/A"));
            rule('-');
            cJSON* banners = cJSON_GetObjectItemCaseSensitive(s, "banners");
            int nb = cJSON_IsArray(banners) ? cJSON_GetArraySize(banners) : 0;
            for (int j = 0; j < nb && j < 32; j++) {
                cJSON* b = cJSON_GetArrayItem(banners, j);
                snprintf(items[total].code, sizeof(items[total].code), "%c%d",
                         (char)(letter + 32), j + 1);
                snprintf(items[total].action_type, sizeof(items[total].action_type), "%s",
                         json_get_str(b, "action_type", ""));
                snprintf(items[total].action_param, sizeof(items[total].action_param), "%s",
                         json_get_str(b, "action_param", ""));
                int discounted = json_get_int(b, "discounted_price", 0);
                printf("  %c%d. %s - %s\n", letter, j + 1,
                       json_get_str(b, "family_name", "N/A"),
                       json_get_str(b, "title", "N/A"));
                printf("     Price: Rp%d\n", discounted);
                printf("     Validity: %s\n", json_get_str(b, "validity", "N/A"));
                rule('-');
                total++;
            }
        }
        printf("00. Kembali\nPilih kode (mis. A1, B2): "); fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) { free(items); cJSON_Delete(res); return; }
        ch[strcspn(ch, "\n")] = 0; lower_in_place(ch);
        if (strcmp(ch, "00") == 0) { free(items); cJSON_Delete(res); return; }
        int found = 0;
        for (int i = 0; i < total; i++) {
            if (strcmp(items[i].code, ch) == 0) {
                found = 1;
                if (strcmp(items[i].action_type, "PDP") == 0 && items[i].action_param[0]) {
                    purchase_flow_by_option_code(items[i].action_param);
                } else if (strcmp(items[i].action_type, "PLP") == 0) {
                    purchase_flow_by_family_code(items[i].action_param);
                } else {
                    printf("Action type %s belum didukung. Param: %s\n",
                           items[i].action_type, items[i].action_param);
                    pause_enter();
                }
                break;
            }
        }
        if (!found) { printf("Kode tidak dikenal.\n"); pause_enter(); }
        free(items);
        cJSON_Delete(res);
    }
}

/* ===== 4. Redeemables Browse ===== */

void show_redeemables_browse(const char* base, const char* key,
                             const char* xdata, const char* sec,
                             const char* id_token) {
    while (1) {
        clear_screen_local();
        rule('='); printf("  Store > Redeemables\n"); rule('=');
        printf("Fetching redeemables...\n");
        cJSON* res = store_get_redeemables(base, key, xdata, sec, id_token, 0);
        if (!json_status_is_success(res)) {
            printf("[-] Gagal ambil redeemables.\n");
            if (res) { char* out = cJSON_Print(res); if (out) { printf("%s\n", out); free(out); } }
            cJSON_Delete(res); pause_enter(); return;
        }
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
        cJSON* cats = data ? cJSON_GetObjectItemCaseSensitive(data, "categories") : NULL;
        int nc = cJSON_IsArray(cats) ? cJSON_GetArraySize(cats) : 0;
        clear_screen_local();
        rule('='); printf("  Store > Redeemables\n"); rule('=');

        StoreItem* items = (StoreItem*)calloc(26 * 32, sizeof(StoreItem));
        if (!items) { printf("[-] OOM.\n"); cJSON_Delete(res); return; }
        int total = 0;

        for (int i = 0; i < nc && i < 26; i++) {
            cJSON* c = cJSON_GetArrayItem(cats, i);
            char letter = (char)('A' + i);
            rule('-');
            printf("%c. Category: %s\n", letter, json_get_str(c, "category_name", "N/A"));
            printf("Code: %s\n", json_get_str(c, "category_code", "N/A"));
            rule('-');
            cJSON* arr = cJSON_GetObjectItemCaseSensitive(c, "redeemables");
            int nr = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
            if (nr == 0) { printf("  (kosong)\n"); continue; }
            for (int j = 0; j < nr && j < 32; j++) {
                cJSON* r = cJSON_GetArrayItem(arr, j);
                snprintf(items[total].code, sizeof(items[total].code), "%c%d",
                         (char)(letter + 32), j + 1);
                snprintf(items[total].action_type, sizeof(items[total].action_type), "%s",
                         json_get_str(r, "action_type", ""));
                snprintf(items[total].action_param, sizeof(items[total].action_param), "%s",
                         json_get_str(r, "action_param", ""));
                time_t vu = (time_t)json_get_double(r, "valid_until", 0);
                struct tm tm; char vs[32] = "--";
                if (vu > 0) { localtime_r(&vu, &tm); strftime(vs, sizeof(vs), "%Y-%m-%d", &tm); }
                printf("  %c%d. %s\n", letter, j + 1, json_get_str(r, "name", "N/A"));
                printf("     Valid Until: %s\n", vs);
                printf("     Action Type: %s\n", items[total].action_type);
                rule('-');
                total++;
            }
        }
        printf("00. Kembali\n99. Menu utama\nPilih kode (mis. A1, B2): "); fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) { free(items); cJSON_Delete(res); return; }
        ch[strcspn(ch, "\n")] = 0; lower_in_place(ch);
        if (strcmp(ch, "00") == 0 || strcmp(ch, "99") == 0) { free(items); cJSON_Delete(res); return; }
        int found = 0;
        for (int i = 0; i < total; i++) {
            if (strcmp(items[i].code, ch) == 0) {
                found = 1;
                if (strcmp(items[i].action_type, "PDP") == 0 && items[i].action_param[0]) {
                    purchase_flow_by_option_code(items[i].action_param);
                } else if (strcmp(items[i].action_type, "PLP") == 0) {
                    purchase_flow_by_family_code(items[i].action_param);
                } else {
                    printf("Action type %s belum didukung. Param: %s\n",
                           items[i].action_type, items[i].action_param);
                    pause_enter();
                }
                break;
            }
        }
        if (!found) { printf("Kode tidak dikenal.\n"); pause_enter(); }
        free(items);
        cJSON_Delete(res);
    }
}

/* ===== 5. Notifications ===== */

void show_notification_browse(const char* base, const char* key,
                              const char* xdata, const char* sec,
                              const char* id_token) {
    extern char acc_tok[4096];
    while (1) {
        clear_screen_local();
        rule('='); printf("  Notifikasi\n"); rule('=');
        printf("Fetching notifications...\n");
        cJSON* res = store_dashboard_segments(base, key, xdata, sec, id_token, acc_tok);
        if (!json_status_is_success(res)) {
            printf("[-] Gagal ambil notifikasi.\n");
            if (res) { char* out = cJSON_Print(res); if (out) { printf("%s\n", out); free(out); } }
            cJSON_Delete(res); pause_enter(); return;
        }
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
        cJSON* notif_wrap = data ? cJSON_GetObjectItemCaseSensitive(data, "notification") : NULL;
        cJSON* list = notif_wrap ? cJSON_GetObjectItemCaseSensitive(notif_wrap, "data") : NULL;
        int n = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;

        clear_screen_local();
        rule('='); printf("  Notifikasi\n"); rule('=');
        int unread = 0;
        if (n == 0) printf("(Tidak ada notifikasi)\n");
        for (int i = 0; i < n; i++) {
            cJSON* e = cJSON_GetArrayItem(list, i);
            int is_read = json_get_int(e, "is_read", 0);
            if (!is_read) unread++;
            printf("%d. [%s] %s\n", i + 1, is_read ? "READ" : "UNREAD",
                   json_get_str(e, "brief_message", ""));
            printf("- Time: %s\n", json_get_str(e, "timestamp", ""));
            printf("- %s\n", json_get_str(e, "full_message", ""));
            rule('-');
        }
        printf("Total: %d | Unread: %d\n", n, unread);
        printf("1. Tandai semua unread sebagai READ\n00. Kembali\n99. Menu utama\nPilih: "); fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) { cJSON_Delete(res); return; }
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0 || strcmp(ch, "99") == 0) { cJSON_Delete(res); return; }
        if (strcmp(ch, "1") == 0) {
            for (int i = 0; i < n; i++) {
                cJSON* e = cJSON_GetArrayItem(list, i);
                if (json_get_int(e, "is_read", 0)) continue;
                const char* nid = json_get_str(e, "notification_id", "");
                if (!nid[0]) continue;
                cJSON* dr = store_get_notification_detail(base, key, xdata, sec, id_token, nid);
                if (json_status_is_success(dr))
                    printf("[+] Marked READ: %s\n", nid);
                cJSON_Delete(dr);
            }
            pause_enter();
        }
        cJSON_Delete(res);
    }
}

/* ===== 6. Menu 5 — beli by option code manual ===== */

void show_buy_by_option_code(void) {
    char code[128];
    clear_screen_local();
    rule('='); printf("  Beli Paket Berdasarkan Option Code\n"); rule('=');
    read_line("Masukkan package_option_code (99=batal): ", code, sizeof(code));
    if (!code[0] || strcmp(code, "99") == 0) { return; }
    purchase_flow_by_option_code(code);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/cJSON.h"
#include "../include/menu/discovery.h"
#include "../include/menu/purchase_flow.h"
#include "../include/client/store.h"
#include "../include/util/json_util.h"
#include "../include/util/nav.h"

extern char active_subs_type[32];

#define W 55

static void fx_clear(void) { printf("\033[H\033[J"); fflush(stdout); }
static void rule(char ch) { for (int i = 0; i < W; i++) putchar(ch); putchar('\n'); }
static void flush_line(void) { int c; while ((c = getchar()) != '\n' && c != EOF); }
static void pause_enter(void) {
    printf("\nTekan Enter untuk melanjutkan...");
    fflush(stdout); flush_line();
}

/* Item generik utk semua hasil discovery — tampung action_type/action_param
 * + label tampil supaya user bisa "1/2/3" → langsung purchase_flow. */
typedef struct {
    char label[160];
    char sub[160];          /* line ke-2 (price / validity / segment) */
    char action_type[24];   /* PLP / PDP / FAMILY_LIST / PACKAGE_DETAIL / OPTION_CODE */
    char action_param[160]; /* family_code atau option_code */
} DItem;

/* Dispatch action_type → purchase_flow yang benar.
 * Mapping: PLP/FAMILY_LIST/FAMILY_CODE → family_code; PDP/PACKAGE_DETAIL/OPTION_CODE → option_code. */
static void dispatch_item(const DItem* it) {
    if (!it->action_param[0]) {
        printf("[-] Action param kosong — tidak bisa dilanjutkan.\n");
        pause_enter(); return;
    }
    const char* t = it->action_type;
    if (strcasecmp(t, "PLP") == 0 ||
        strcasecmp(t, "FAMILY_LIST") == 0 ||
        strcasecmp(t, "FAMILY_CODE") == 0) {
        purchase_flow_by_family_code(it->action_param);
    } else if (strcasecmp(t, "PDP") == 0 ||
               strcasecmp(t, "PACKAGE_DETAIL") == 0 ||
               strcasecmp(t, "OPTION_CODE") == 0) {
        purchase_flow_by_option_code(it->action_param);
    } else {
        /* Fallback: kebanyakan response dari store endpoint pakai family_code
         * sebagai action_param. Coba sebagai family_code dulu. */
        printf("[i] Action type \"%s\" tidak dikenal — coba sebagai family_code.\n", t);
        purchase_flow_by_family_code(it->action_param);
    }
}

static int prompt_select_or_back(int n) {
    while (1) {
        printf("Pilih nomor (1-%d), 00 kembali, 99 menu utama: ", n);
        fflush(stdout);
        char buf[16];
        if (!fgets(buf, sizeof(buf), stdin)) return -1;
        buf[strcspn(buf, "\n")] = 0;
        if (strcmp(buf, "00") == 0) return -1;
        if (strcmp(buf, "99") == 0) { nav_trigger_goto_main(); return -1; }
        int sel = atoi(buf);
        if (sel >= 1 && sel <= n) return sel;
        printf("Pilihan tidak valid.\n");
    }
}

static void render_items(const DItem* items, int n) {
    for (int i = 0; i < n; i++) {
        printf("%2d. %s\n", i + 1, items[i].label);
        if (items[i].sub[0]) printf("    %s\n", items[i].sub);
        if (items[i].action_param[0])
            printf("    [%s] %s\n", items[i].action_type, items[i].action_param);
        rule('-');
    }
}

/* ===== 1. Search by keyword =====
 * POST /api/v9/xl-stores/options/search dengan text_search=<keyword>.
 * Response: data.results[] (atau results_price_only/results_quota_only). */
static void show_search_keyword(const char* base, const char* key,
                                const char* xdata, const char* sec,
                                const char* id_token) {
    while (1) {
        fx_clear();
        rule('='); printf("  Discovery > Search Family Code (Keyword)\n"); rule('=');
        printf("Tip: ketik nama paket / family / kuota (mis. 'akrab', 'youtube',\n"
               "'circle', '50gb'). Kosongkan + Enter utk batal.\n\n");
        printf("Keyword (00 kembali, 99 menu utama): ");
        fflush(stdout);
        char kw[128];
        if (!fgets(kw, sizeof(kw), stdin)) return;
        kw[strcspn(kw, "\n")] = 0;
        if (kw[0] == '\0' || strcmp(kw, "00") == 0) return;
        if (strcmp(kw, "99") == 0) { nav_trigger_goto_main(); return; }

        printf("\nMencari...\n");
        cJSON* res = store_search_packages(base, key, xdata, sec, id_token,
                                           active_subs_type, 0, kw);
        if (!json_status_is_success(res)) {
            printf("[-] Gagal cari paket.\n");
            if (res) { char* o = cJSON_Print(res); if (o) { printf("%s\n", o); free(o); } }
            cJSON_Delete(res); pause_enter(); continue;
        }
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
        cJSON* results = data ? cJSON_GetObjectItemCaseSensitive(data, "results") : NULL;
        if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0)
            results = data ? cJSON_GetObjectItemCaseSensitive(data, "results_price_only") : NULL;
        if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0)
            results = data ? cJSON_GetObjectItemCaseSensitive(data, "results_quota_only") : NULL;
        int n = cJSON_IsArray(results) ? cJSON_GetArraySize(results) : 0;
        if (n == 0) {
            printf("Tidak ada hasil untuk \"%s\".\n", kw);
            cJSON_Delete(res); pause_enter(); continue;
        }
        if (n > 60) n = 60;

        DItem* items = (DItem*)calloc((size_t)n, sizeof(DItem));
        if (!items) { printf("[-] OOM.\n"); cJSON_Delete(res); pause_enter(); return; }
        for (int i = 0; i < n; i++) {
            cJSON* r = cJSON_GetArrayItem(results, i);
            int dp = json_get_int(r, "discounted_price", 0);
            int op = json_get_int(r, "original_price", 0);
            snprintf(items[i].label, sizeof(items[i].label), "%s — %s",
                     json_get_str(r, "family_name", "?"),
                     json_get_str(r, "title", "?"));
            if (op > 0 && op != dp)
                snprintf(items[i].sub, sizeof(items[i].sub),
                         "Rp%d (was Rp%d) | %s", dp, op,
                         json_get_str(r, "validity", "-"));
            else
                snprintf(items[i].sub, sizeof(items[i].sub), "Rp%d | %s",
                         dp, json_get_str(r, "validity", "-"));
            snprintf(items[i].action_type, sizeof(items[i].action_type), "%s",
                     json_get_str(r, "action_type", "FAMILY_CODE"));
            snprintf(items[i].action_param, sizeof(items[i].action_param), "%s",
                     json_get_str(r, "action_param", ""));
        }
        cJSON_Delete(res);

        fx_clear();
        rule('='); printf("  Hasil cari \"%s\" (%d paket)\n", kw, n); rule('=');
        render_items(items, n);
        int sel = prompt_select_or_back(n);
        if (sel >= 1) dispatch_item(&items[sel - 1]);
        free(items);
        if (nav_should_return()) return;
    }
}

/* ===== 2. Recommendation =====
 * POST /api/v8/xl-stores/options/recommendation. Response:
 * data.recommendations[] = [{ category, title, banners: [...], buble_text }]. */
static void show_recommendations(const char* base, const char* key,
                                 const char* xdata, const char* sec,
                                 const char* id_token) {
    fx_clear();
    rule('='); printf("  Discovery > Rekomendasi Paket\n"); rule('=');
    printf("Mengambil rekomendasi personal dari server...\n");
    cJSON* res = store_get_recommendations(base, key, xdata, sec, id_token);
    if (!json_status_is_success(res)) {
        printf("[-] Gagal ambil rekomendasi.\n");
        if (res) { char* o = cJSON_Print(res); if (o) { printf("%s\n", o); free(o); } }
        cJSON_Delete(res); pause_enter(); return;
    }
    cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
    cJSON* recs = data ? cJSON_GetObjectItemCaseSensitive(data, "recommendations") : NULL;
    int nr = cJSON_IsArray(recs) ? cJSON_GetArraySize(recs) : 0;
    if (nr == 0) {
        printf("Tidak ada rekomendasi (server tidak mengembalikan data).\n");
        cJSON_Delete(res); pause_enter(); return;
    }

    /* Flatten semua banner di tiap kategori jadi list 1-N. */
    DItem* items = (DItem*)calloc(200, sizeof(DItem));
    if (!items) { printf("[-] OOM.\n"); cJSON_Delete(res); pause_enter(); return; }
    int total = 0;

    fx_clear();
    rule('='); printf("  Discovery > Rekomendasi Paket\n"); rule('=');
    for (int i = 0; i < nr && total < 200; i++) {
        cJSON* g = cJSON_GetArrayItem(recs, i);
        const char* cat = json_get_str(g, "category", "");
        const char* gtitle = json_get_str(g, "title", "");
        printf("\n[%s] %s\n", cat[0] ? cat : "GROUP", gtitle);
        rule('-');
        cJSON* banners = cJSON_GetObjectItemCaseSensitive(g, "banners");
        int nb = cJSON_IsArray(banners) ? cJSON_GetArraySize(banners) : 0;
        for (int j = 0; j < nb && total < 200; j++) {
            cJSON* b = cJSON_GetArrayItem(banners, j);
            int dp = json_get_int(b, "discounted_price", 0);
            int op = json_get_int(b, "original_price", 0);
            snprintf(items[total].label, sizeof(items[total].label), "%s",
                     json_get_str(b, "title", "?"));
            if (dp > 0) {
                if (op > 0 && op != dp)
                    snprintf(items[total].sub, sizeof(items[total].sub),
                             "Rp%d (was Rp%d) | %s", dp, op,
                             json_get_str(b, "validity", "-"));
                else
                    snprintf(items[total].sub, sizeof(items[total].sub),
                             "Rp%d | %s", dp,
                             json_get_str(b, "validity", "-"));
            } else {
                snprintf(items[total].sub, sizeof(items[total].sub), "%s",
                         json_get_str(b, "buble_text", json_get_str(b, "validity", "-")));
            }
            snprintf(items[total].action_type, sizeof(items[total].action_type), "%s",
                     json_get_str(b, "action_type", "FAMILY_CODE"));
            snprintf(items[total].action_param, sizeof(items[total].action_param), "%s",
                     json_get_str(b, "action_param", ""));
            printf("%2d. %s\n    %s\n    [%s] %s\n", total + 1,
                   items[total].label, items[total].sub,
                   items[total].action_type, items[total].action_param);
            total++;
        }
    }
    cJSON_Delete(res);

    if (total == 0) {
        printf("\nKategori ada tapi banner kosong.\n");
        free(items); pause_enter(); return;
    }
    rule('=');
    int sel = prompt_select_or_back(total);
    if (sel >= 1) dispatch_item(&items[sel - 1]);
    free(items);
}

/* ===== 3. Banners =====
 * POST /store/api/v8/banners → data.banners[] (full info, sering FAMILY_LIST)
 * POST personalization/dynamic-banners → data[] berisi banners[] per placement. */
static void collect_banners_obj(cJSON* arr, DItem* items, int* total, int max,
                                const char* source_label) {
    int n = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
    for (int i = 0; i < n && *total < max; i++) {
        cJSON* b = cJSON_GetArrayItem(arr, i);
        const char* at = json_get_str(b, "action_type", "");
        const char* ap = json_get_str(b, "action_param", "");
        if (!ap[0]) continue; /* skip banner non-actionable */
        snprintf(items[*total].label, sizeof(items[*total].label),
                 "[%s] %s", source_label,
                 json_get_str(b, "title",
                              json_get_str(b, "category",
                                           json_get_str(b, "promo_description", "(banner)"))));
        int dp = json_get_int(b, "discounted_price", json_get_int(b, "price", 0));
        int op = json_get_int(b, "original_price", 0);
        if (dp > 0) {
            if (op > 0 && op != dp)
                snprintf(items[*total].sub, sizeof(items[*total].sub),
                         "Rp%d (was Rp%d)", dp, op);
            else
                snprintf(items[*total].sub, sizeof(items[*total].sub),
                         "Rp%d", dp);
        } else {
            snprintf(items[*total].sub, sizeof(items[*total].sub), "%s",
                     json_get_str(b, "promo_description",
                                  json_get_str(b, "message", "")));
        }
        snprintf(items[*total].action_type, sizeof(items[*total].action_type),
                 "%s", at[0] ? at : "FAMILY_CODE");
        snprintf(items[*total].action_param, sizeof(items[*total].action_param),
                 "%s", ap);
        (*total)++;
    }
}

static void show_promo_banners(const char* base, const char* key,
                               const char* xdata, const char* sec,
                               const char* id_token) {
    fx_clear();
    rule('='); printf("  Discovery > Banner Promo\n"); rule('=');
    printf("Mengambil banner store + dynamic banners...\n");

    DItem* items = (DItem*)calloc(120, sizeof(DItem));
    if (!items) { printf("[-] OOM.\n"); pause_enter(); return; }
    int total = 0;

    /* (a) /store/api/v8/banners */
    cJSON* res1 = store_get_home_banners(base, key, xdata, sec, id_token);
    if (json_status_is_success(res1)) {
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res1, "data");
        cJSON* banners = data ? cJSON_GetObjectItemCaseSensitive(data, "banners") : NULL;
        collect_banners_obj(banners, items, &total, 120, "STORE");
    } else {
        printf("[i] /store/api/v8/banners: gagal/empty.\n");
    }
    cJSON_Delete(res1);

    /* (b) personalization/dynamic-banners — coba placement HOME + STORE */
    const char* placements[] = { "HOME", "STORE", "QUOTA_DETAIL" };
    for (size_t pi = 0; pi < sizeof(placements) / sizeof(placements[0]); pi++) {
        cJSON* res2 = store_get_dynamic_banners(base, key, xdata, sec, id_token,
                                                placements[pi]);
        if (!json_status_is_success(res2)) { cJSON_Delete(res2); continue; }
        cJSON* data2 = cJSON_GetObjectItemCaseSensitive(res2, "data");
        /* Bisa array of {placement_type, banners[]} atau langsung banners */
        if (cJSON_IsArray(data2)) {
            int nd = cJSON_GetArraySize(data2);
            for (int i = 0; i < nd; i++) {
                cJSON* g = cJSON_GetArrayItem(data2, i);
                cJSON* bb = cJSON_GetObjectItemCaseSensitive(g, "banners");
                collect_banners_obj(bb, items, &total, 120, placements[pi]);
            }
        } else if (data2) {
            cJSON* bb = cJSON_GetObjectItemCaseSensitive(data2, "banners");
            collect_banners_obj(bb, items, &total, 120, placements[pi]);
        }
        cJSON_Delete(res2);
    }

    fx_clear();
    rule('='); printf("  Discovery > Banner Promo (%d aktif)\n", total); rule('=');
    if (total == 0) {
        printf("Tidak ada banner aktif yang bisa di-purchase.\n");
        free(items); pause_enter(); return;
    }
    render_items(items, total);
    int sel = prompt_select_or_back(total);
    if (sel >= 1) dispatch_item(&items[sel - 1]);
    free(items);
}

/* ===== 4. Loyalty Tier Rewards =====
 * POST /gamification/api/v8/loyalties/tiering/rewards-catalog → data.tiers[].rewards[].
 * Reward umumnya tidak dibeli pakai purchase biasa — pakai redeem (point).
 * Untuk discovery family code: tampilkan info reward (code, title, price, validity).
 * Kalau reward benefit_code-nya berupa OPTION/FAMILY, dispatch via purchase_flow. */
static void show_tier_rewards(const char* base, const char* key,
                              const char* xdata, const char* sec,
                              const char* id_token) {
    fx_clear();
    rule('='); printf("  Discovery > Loyalty Tier Rewards\n"); rule('=');
    printf("Mengambil rewards catalog (per tier)...\n");
    cJSON* res = store_get_tier_rewards(base, key, xdata, sec, id_token);
    if (!json_status_is_success(res)) {
        printf("[-] Gagal ambil rewards catalog.\n"
               "    Endpoint ini biasanya cuma jalan utk akun PRIO/PRIOHYBRID.\n"
               "    Subs type aktif: %s\n", active_subs_type);
        if (res) { char* o = cJSON_Print(res); if (o) { printf("%s\n", o); free(o); } }
        cJSON_Delete(res); pause_enter(); return;
    }
    cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
    cJSON* tiers = data ? cJSON_GetObjectItemCaseSensitive(data, "tiers") : NULL;
    int nt = cJSON_IsArray(tiers) ? cJSON_GetArraySize(tiers) : 0;
    if (nt == 0) {
        printf("Tidak ada tier reward (akun belum punya tier).\n");
        cJSON_Delete(res); pause_enter(); return;
    }

    DItem* items = (DItem*)calloc(200, sizeof(DItem));
    if (!items) { printf("[-] OOM.\n"); cJSON_Delete(res); pause_enter(); return; }
    int total = 0;

    fx_clear();
    rule('='); printf("  Discovery > Loyalty Tier Rewards\n"); rule('=');
    for (int i = 0; i < nt && total < 200; i++) {
        cJSON* tier = cJSON_GetArrayItem(tiers, i);
        const char* tname = json_get_str(tier, "name",
                                         json_get_str(tier, "tier_name", "TIER"));
        printf("\n[Tier: %s]\n", tname);
        rule('-');
        cJSON* rewards = cJSON_GetObjectItemCaseSensitive(tier, "rewards");
        if (!cJSON_IsArray(rewards))
            rewards = cJSON_GetObjectItemCaseSensitive(tier, "rewards_catalog");
        int nr = cJSON_IsArray(rewards) ? cJSON_GetArraySize(rewards) : 0;
        if (nr == 0) { printf("  (kosong)\n"); continue; }
        for (int j = 0; j < nr && total < 200; j++) {
            cJSON* r = cJSON_GetArrayItem(rewards, j);
            const char* code = json_get_str(r, "code", "");
            const char* bcode = json_get_str(r, "benefit_code", "");
            int price = json_get_int(r, "price", 0);
            snprintf(items[total].label, sizeof(items[total].label), "%s",
                     json_get_str(r, "title", "?"));
            snprintf(items[total].sub, sizeof(items[total].sub),
                     "%d pts | %s | code=%s | benefit=%s",
                     price, json_get_str(r, "validity", "-"),
                     code[0] ? code : "-", bcode[0] ? bcode : "-");
            /* Reward tidak dispatch ke purchase_flow (perlu redeem terpisah).
             * Di sini cuma tampilan info — user bisa save code manual. */
            snprintf(items[total].action_type, sizeof(items[total].action_type),
                     "REWARD");
            snprintf(items[total].action_param, sizeof(items[total].action_param),
                     "%s", code);
            printf("%2d. %s\n    %s\n", total + 1,
                   items[total].label, items[total].sub);
            total++;
        }
    }
    cJSON_Delete(res);

    if (total == 0) {
        printf("\nSemua tier kosong.\n");
        free(items); pause_enter(); return;
    }
    rule('=');
    printf("Catatan: reward tier ditukar pakai POIN (tidak via purchase normal).\n");
    printf("Pilih nomor utk lihat code (saved info utk dipakai redeem manual).\n");
    int sel = prompt_select_or_back(total);
    if (sel >= 1) {
        const DItem* it = &items[sel - 1];
        fx_clear();
        rule('='); printf("  Reward Detail\n"); rule('=');
        printf("Title : %s\nCode  : %s\nInfo  : %s\n",
               it->label, it->action_param, it->sub);
        rule('=');
        pause_enter();
    }
    free(items);
}

/* ===== Top-level menu ===== */

void show_discovery_menu(const char* base_api, const char* api_key,
                         const char* xdata_key, const char* x_api_secret,
                         const char* id_token) {
    if (!id_token || !id_token[0]) { printf("[-] Belum login.\n"); return; }
    while (1) {
        fx_clear();
        rule('='); printf("  DISCOVERY PAKET TERSEMBUNYI\n"); rule('=');
        printf("Subs type: %s\n", active_subs_type);
        rule('-');
        printf("1. Cari family code (keyword)\n"
               "2. Lihat rekomendasi paket\n"
               "3. Lihat banner promo (store + dynamic)\n"
               "4. Loyalty tier rewards (PRIO/PRIOHYBRID)\n"
               "-------------------------------------------------------\n"
               "00. Kembali\n"
               "99. Menu utama\n"
               "Pilihan: ");
        fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) return;
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0) return;
        if (strcmp(ch, "99") == 0) { nav_trigger_goto_main(); return; }
        if (strcmp(ch, "1") == 0)
            show_search_keyword(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "2") == 0)
            show_recommendations(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "3") == 0)
            show_promo_banners(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "4") == 0)
            show_tier_rewards(base_api, api_key, xdata_key, x_api_secret, id_token);
        if (nav_should_return()) return;
    }
}

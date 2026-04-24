/* ---------------------------------------------------------------------------
 * Auto Buy
 *
 * Fitur:
 *   - CRUD entry Auto Buy di /etc/engsel/auto_buy.json
 *   - Worker loop yang cek kuota aktif setiap N detik; jika remaining (MB)
 *     jatuh di bawah threshold, trigger purchase otomatis (BALANCE atau
 *     BALANCE+DECOY dengan overwrite).
 *   - Mode CLI `engsel --auto-buy` untuk jalan di background (procd service,
 *     nohup, dsb.); menu utama bisa cek status via PID file.
 *
 * File yang dipakai:
 *   /etc/engsel/auto_buy.json        — konfigurasi entry
 *   /var/run/engsel-autobuy.pid      — PID worker yang aktif
 *   /var/run/engsel-autobuy.state    — state JSON (last tick, last trigger)
 *   /var/log/engsel-autobuy.log      — log trigger
 * ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/cJSON.h"
#include "../include/menu/auto_buy.h"
#include "../include/client/engsel.h"
#include "../include/util/json_util.h"
#include "../include/util/file_util.h"

static void ab_clear(void) { printf("\033[H\033[J"); fflush(stdout); }

/* Helper input (duplicate dari menu lain yang static). */
static void pause_enter(void) {
    printf("Tekan Enter untuk melanjutkan...");
    fflush(stdout);
    int c; while ((c = getchar()) != '\n' && c != EOF) {}
}

static void read_line(const char* prompt, char* buf, size_t n) {
    if (prompt && *prompt) { printf("%s", prompt); fflush(stdout); }
    if (!fgets(buf, (int)n, stdin)) { buf[0] = 0; return; }
    buf[strcspn(buf, "\n")] = 0;
}

extern char active_subs_type[32];
extern int  is_logged_in;
extern int  active_account_idx;
extern cJSON* g_tokens_arr;
extern char id_tok[4096];   /* token login aktif, diisi di authenticate_and_fetch_balance */
extern char acc_tok[4096];
extern void ensure_token_fresh(cJSON* tokens_arr, const char* B_CIAM,
                               const char* B_API, const char* B_AUTH,
                               const char* UA, const char* API_KEY,
                               const char* XDATA_KEY, const char* X_API_SEC);
extern void authenticate_and_fetch_balance(cJSON* tokens_arr, const char* B_CIAM,
                                           const char* B_API, const char* B_AUTH,
                                           const char* UA, const char* API_KEY,
                                           const char* XDATA_KEY, const char* X_API_SEC);
extern cJSON* do_family_bruteforce(const char* B_API, const char* API_KEY,
                                   const char* XDATA_KEY, const char* X_API_SEC,
                                   const char* id_tok, const char* f_code,
                                   int is_ent_override, const char* mig_override);
extern cJSON* fetch_decoy_package(const char* subs_type, const char* B_API,
                                  const char* API_KEY, const char* XDATA_KEY,
                                  const char* X_API_SEC, const char* id_tok);

static const char* AB_CONFIG_PATH = "/etc/engsel/auto_buy.json";
static const char* AB_PID_PATH    = "/var/run/engsel-autobuy.pid";
static const char* AB_STATE_PATH  = "/var/run/engsel-autobuy.state";
static const char* AB_LOG_PATH    = "/var/log/engsel-autobuy.log";

static volatile sig_atomic_t g_ab_stop = 0;

/* ---------- helpers ---------- */

static void ab_log(const char* fmt, ...) {
    char stamp[32];
    time_t now = time(NULL);
    struct tm tmv; localtime_r(&now, &tmv);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);

    FILE* f = fopen(AB_LOG_PATH, "a");
    if (!f) return;
    fprintf(f, "[%s] ", stamp);
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static cJSON* ab_load(void) {
    size_t sz = 0;
    char* raw = file_read_all(AB_CONFIG_PATH, &sz);
    if (!raw || sz == 0) { free(raw); return cJSON_CreateArray(); }
    cJSON* arr = cJSON_Parse(raw); free(raw);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return cJSON_CreateArray(); }
    return arr;
}

static int ab_save(cJSON* arr) {
    char* out = cJSON_Print(arr);
    if (!out) return -1;
    int rc = file_write_atomic(AB_CONFIG_PATH, out);
    free(out);
    return rc;
}

/* Baca PID worker. Return >0 kalau aktif, 0 kalau mati/tidak ada. */
static pid_t ab_read_pid(void) {
    size_t sz = 0;
    char* raw = file_read_all(AB_PID_PATH, &sz);
    if (!raw) return 0;
    pid_t p = (pid_t)atoi(raw);
    free(raw);
    if (p <= 0) return 0;
    if (kill(p, 0) == 0) return p;
    /* stale */
    unlink(AB_PID_PATH);
    return 0;
}

static int ab_write_pid(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    return file_write_atomic(AB_PID_PATH, buf);
}

static void ab_write_state(const char* last_trigger_name, int iterations,
                           const char* note) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "pid",         (double)getpid());
    cJSON_AddNumberToObject(o, "last_tick",   (double)time(NULL));
    cJSON_AddNumberToObject(o, "iterations",  (double)iterations);
    if (last_trigger_name)
        cJSON_AddStringToObject(o, "last_trigger_name", last_trigger_name);
    if (note) cJSON_AddStringToObject(o, "note", note);
    char* s = cJSON_Print(o);
    if (s) { file_write_atomic(AB_STATE_PATH, s); free(s); }
    cJSON_Delete(o);
}

/* ---------- add-entry UI ---------- */

/* Pilih 1 opsi dari family -> tulis ke out_* */
typedef struct {
    char family_code[128];
    char variant_code[128];
    char option_code[128];
    int  order;
    int  price;
    char name[256];
    int  is_enterprise;
    char migration_type[64];
} ab_option_t;

static int ab_pick_option(const char* base_api, const char* api_key,
                          const char* xdata_key, const char* x_api_secret,
                          const char* id_token, const char* family_code,
                          ab_option_t* out) {
    printf("[*] Fetching family dari XL Store...\n");
    /* Migration types sinkron dengan do_family_bruteforce (main.c:232) */
    const char* migs[] = { "NONE", "PRE_TO_PRIOH", "PRIOH_TO_PRIO", "PRIO_TO_PRIOH" };
    int ents[] = { 0, 1 };
    cJSON* fam = NULL; int ent_found = 0; const char* mig_found = "NONE";
    for (size_t i = 0; i < sizeof(migs)/sizeof(migs[0]) && !fam; i++) {
        for (size_t j = 0; j < sizeof(ents)/sizeof(ents[0]) && !fam; j++) {
            cJSON* r = get_family(base_api, api_key, xdata_key, x_api_secret,
                                  id_token, family_code, ents[j], migs[i]);
            if (!r) continue;
            cJSON* data = cJSON_GetObjectItem(r, "data");
            cJSON* variants = data ? cJSON_GetObjectItem(data, "package_variants") : NULL;
            if (variants && cJSON_GetArraySize(variants) > 0) {
                fam = r; ent_found = ents[j]; mig_found = migs[i];
            } else {
                cJSON_Delete(r);
            }
        }
    }
    if (!fam) { printf("[-] Family tidak ditemukan.\n"); return -1; }

    /* Flatten */
    typedef struct { const char* vc; int ord; const char* oc; const char* name; int price; } it_t;
    it_t items[256]; int n = 0;
    cJSON* data = cJSON_GetObjectItem(fam, "data");
    cJSON* variants = cJSON_GetObjectItem(data, "package_variants");
    cJSON* v;
    cJSON_ArrayForEach(v, variants) {
        const char* vc = json_get_str(v, "package_variant_code", "");
        cJSON* opts = cJSON_GetObjectItem(v, "package_options");
        cJSON* o;
        cJSON_ArrayForEach(o, opts) {
            if (n >= 256) break;
            items[n].vc    = vc;
            items[n].ord   = cJSON_GetObjectItem(o, "order") ? cJSON_GetObjectItem(o, "order")->valueint : 0;
            items[n].oc    = json_get_str(o, "package_option_code", "");
            items[n].name  = json_get_str(o, "name", "?");
            items[n].price = cJSON_GetObjectItem(o, "price") ? cJSON_GetObjectItem(o, "price")->valueint : 0;
            n++;
        }
    }
    if (n == 0) { printf("[-] Family ini tidak punya opsi.\n"); cJSON_Delete(fam); return -1; }

    printf("-------------------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        printf("%2d. %s  (Rp %d)\n", i + 1, items[i].name, items[i].price);
    }
    printf("-------------------------------------------------------\n");
    char pick[16]; read_line("Pilih nomor opsi (99=batal): ", pick, sizeof(pick));
    if (strcmp(pick, "99") == 0) { cJSON_Delete(fam); return -1; }
    int idx = atoi(pick);
    if (idx < 1 || idx > n) {
        printf("[-] Nomor di luar range.\n"); cJSON_Delete(fam); return -1;
    }

    snprintf(out->family_code,  sizeof(out->family_code),  "%s", family_code);
    snprintf(out->variant_code, sizeof(out->variant_code), "%s", items[idx-1].vc);
    snprintf(out->option_code,  sizeof(out->option_code),  "%s", items[idx-1].oc);
    snprintf(out->name,         sizeof(out->name),         "%s", items[idx-1].name);
    out->order         = items[idx-1].ord;
    out->price         = items[idx-1].price;
    out->is_enterprise = ent_found;
    snprintf(out->migration_type, sizeof(out->migration_type), "%s", mig_found);
    cJSON_Delete(fam);
    return 0;
}

static cJSON* ab_option_to_json(const ab_option_t* o) {
    cJSON* j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "family_code",    o->family_code);
    cJSON_AddStringToObject(j, "variant_code",   o->variant_code);
    cJSON_AddStringToObject(j, "option_code",    o->option_code);
    cJSON_AddNumberToObject(j, "order",          o->order);
    cJSON_AddNumberToObject(j, "price",          o->price);
    cJSON_AddStringToObject(j, "name",           o->name);
    cJSON_AddBoolToObject  (j, "is_enterprise",  o->is_enterprise);
    cJSON_AddStringToObject(j, "migration_type", o->migration_type);
    return j;
}

/* Pilih decoy: 'd' default, 'c' custom input, 'a' dari Custom Decoy tersimpan */
static int ab_pick_decoy(const char* base_api, const char* api_key,
                         const char* xdata_key, const char* x_api_secret,
                         const char* id_token,
                         char* out_source, size_t out_source_sz,
                         ab_option_t* out_opt) {
    printf("\n-- Pilih Decoy --\n"
           "d. Default bawaan (decoy_data/*.json sesuai tipe kartu)\n"
           "c. Custom - input family code decoy baru\n"
           "a. Pilih dari daftar Custom Decoy tersimpan (tipe %s saja)\n"
           "99. Batal\n"
           "Pilihan: ", active_subs_type);
    fflush(stdout);
    char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) return -1;
    ch[strcspn(ch, "\n")] = 0;

    if (strcmp(ch, "99") == 0) return -1;
    if (strcmp(ch, "d") == 0 || strcmp(ch, "D") == 0) {
        snprintf(out_source, out_source_sz, "default");
        memset(out_opt, 0, sizeof(*out_opt));
        return 0;
    }
    if (strcmp(ch, "c") == 0 || strcmp(ch, "C") == 0) {
        char code[256]; read_line("Family code decoy: ", code, sizeof(code));
        if (strlen(code) == 0) return -1;
        if (ab_pick_option(base_api, api_key, xdata_key, x_api_secret,
                           id_token, code, out_opt) != 0) return -1;
        snprintf(out_source, out_source_sz, "custom_inline");
        return 0;
    }
    if (strcmp(ch, "a") == 0 || strcmp(ch, "A") == 0) {
        /* Baca /etc/engsel/custom_decoy.json, filter subs_type */
        size_t sz = 0;
        char* raw = file_read_all("/etc/engsel/custom_decoy.json", &sz);
        if (!raw) { printf("[-] File custom_decoy.json belum ada.\n"); return -1; }
        cJSON* cfg = cJSON_Parse(raw); free(raw);
        if (!cfg) { printf("[-] JSON custom_decoy parse error.\n"); return -1; }
        cJSON* ents = cJSON_GetObjectItem(cfg, "entries");
        if (!ents || !cJSON_IsArray(ents)) {
            printf("[-] Belum ada entry custom_decoy.\n"); cJSON_Delete(cfg); return -1;
        }
        int real_map[128]; int nvis = 0;
        int total = cJSON_GetArraySize(ents);
        for (int i = 0; i < total; i++) {
            cJSON* e = cJSON_GetArrayItem(ents, i);
            if (strcmp(json_get_str(e, "subs_type", ""), active_subs_type) == 0) {
                if (nvis >= (int)(sizeof(real_map)/sizeof(real_map[0]))) break;
                real_map[nvis++] = i;
            }
        }
        if (nvis == 0) {
            printf("[-] Tidak ada custom decoy untuk tipe %s.\n", active_subs_type);
            cJSON_Delete(cfg); return -1;
        }
        printf("-------------------------------------------------------\n");
        for (int i = 0; i < nvis; i++) {
            cJSON* e = cJSON_GetArrayItem(ents, real_map[i]);
            printf("%2d. %s\n", i + 1, json_get_str(e, "name", "?"));
        }
        printf("-------------------------------------------------------\n");
        char pick[16]; read_line("Pilih nomor (99=batal): ", pick, sizeof(pick));
        if (strcmp(pick, "99") == 0) { cJSON_Delete(cfg); return -1; }
        int v = atoi(pick);
        if (v < 1 || v > nvis) { printf("[-] Nomor di luar range.\n"); cJSON_Delete(cfg); return -1; }
        cJSON* e = cJSON_GetArrayItem(ents, real_map[v-1]);

        memset(out_opt, 0, sizeof(*out_opt));
        snprintf(out_opt->family_code,    sizeof(out_opt->family_code),    "%s", json_get_str(e, "family_code", ""));
        snprintf(out_opt->variant_code,   sizeof(out_opt->variant_code),   "%s", json_get_str(e, "variant_code", ""));
        snprintf(out_opt->name,           sizeof(out_opt->name),           "%s", json_get_str(e, "name", ""));
        snprintf(out_opt->migration_type, sizeof(out_opt->migration_type), "%s", json_get_str(e, "migration_type", "NONE"));
        out_opt->order         = cJSON_GetObjectItem(e, "order") ? cJSON_GetObjectItem(e, "order")->valueint : 0;
        out_opt->is_enterprise = cJSON_IsTrue(cJSON_GetObjectItem(e, "is_enterprise")) ? 1 : 0;
        /* option_code tidak disimpan di custom_decoy.json (decoy biasanya dipakai
         * via bruteforce variant+order di fetch time); kita biarkan kosong dan
         * worker akan resolve saat trigger. */
        snprintf(out_source, out_source_sz, "custom_saved");
        cJSON_Delete(cfg);
        return 0;
    }
    return -1;
}

static void ab_add(const char* base_api, const char* api_key,
                   const char* xdata_key, const char* x_api_secret,
                   const char* id_token) {
    char code[256]; read_line("Family code target auto-buy (99=batal): ", code, sizeof(code));
    if (strcmp(code, "99") == 0 || strlen(code) == 0) return;

    ab_option_t tgt;
    if (ab_pick_option(base_api, api_key, xdata_key, x_api_secret,
                       id_token, code, &tgt) != 0) { pause_enter(); return; }

    char s[32];
    read_line("Threshold sisa kuota (MB): ", s, sizeof(s));
    int thr_mb = atoi(s); if (thr_mb <= 0) { printf("[-] Threshold invalid.\n"); pause_enter(); return; }
    read_line("Interval cek (detik, min 30): ", s, sizeof(s));
    int interval = atoi(s); if (interval < 30) { printf("[-] Interval minimal 30 detik.\n"); pause_enter(); return; }

    printf("\n-- Metode Pembayaran --\n"
           "1. BALANCE (Pulsa biasa)\n"
           "2. BALANCE + DECOY (Pulsa+Decoy dengan overwrite)\n"
           "99. Batal\n"
           "Pilihan: ");
    fflush(stdout);
    char pm[16]; if (!fgets(pm, sizeof(pm), stdin)) return;
    pm[strcspn(pm, "\n")] = 0;

    char payment_method[32];
    char decoy_source[32] = "";
    ab_option_t dec; memset(&dec, 0, sizeof(dec));

    if (strcmp(pm, "1") == 0) {
        snprintf(payment_method, sizeof(payment_method), "BALANCE");
    } else if (strcmp(pm, "2") == 0) {
        snprintf(payment_method, sizeof(payment_method), "BALANCE_DECOY");
        if (ab_pick_decoy(base_api, api_key, xdata_key, x_api_secret,
                          id_token, decoy_source, sizeof(decoy_source), &dec) != 0) {
            printf("[!] Dibatalkan.\n"); pause_enter(); return;
        }
    } else {
        return;
    }

    cJSON* arr = ab_load();
    cJSON* e = cJSON_CreateObject();
    cJSON_AddStringToObject(e, "name",                   tgt.name);
    cJSON_AddStringToObject(e, "family_code",            tgt.family_code);
    cJSON_AddStringToObject(e, "variant_code",           tgt.variant_code);
    cJSON_AddStringToObject(e, "option_code",            tgt.option_code);
    cJSON_AddNumberToObject(e, "order",                  tgt.order);
    cJSON_AddNumberToObject(e, "price",                  tgt.price);
    cJSON_AddBoolToObject  (e, "is_enterprise",          tgt.is_enterprise);
    cJSON_AddStringToObject(e, "migration_type",         tgt.migration_type);
    cJSON_AddNumberToObject(e, "threshold_mb",           thr_mb);
    cJSON_AddNumberToObject(e, "interval_sec",           interval);
    cJSON_AddStringToObject(e, "payment_method",         payment_method);
    cJSON_AddStringToObject(e, "decoy_source",           decoy_source);
    cJSON_AddStringToObject(e, "subs_type_when_added",   active_subs_type);
    cJSON_AddBoolToObject  (e, "enabled",                1);
    if (strcmp(payment_method, "BALANCE_DECOY") == 0) {
        cJSON_AddItemToObject(e, "decoy", ab_option_to_json(&dec));
    }
    cJSON_AddItemToArray(arr, e);

    if (ab_save(arr) == 0) printf("[+] Entry tersimpan: %s\n", tgt.name);
    else                    printf("[-] Gagal simpan.\n");
    cJSON_Delete(arr);
    pause_enter();
}

/* ---------- worker ---------- */

static void ab_on_term(int sig) { (void)sig; g_ab_stop = 1; }

/* Cek kuota aktif user: sum benefits.remaining (bytes) untuk quota yang
 * family_code-nya match. Return -1 kalau tidak ketemu, else total dalam byte. */
static long long ab_check_remaining(const char* base_api, const char* api_key,
                                    const char* xdata_key, const char* x_api_secret,
                                    const char* id_token, const char* family_code) {
    cJSON* q = get_quota(base_api, api_key, xdata_key, x_api_secret, id_token);
    if (!q) return -1;
    long long total_rem = -1;
    cJSON* data = cJSON_GetObjectItem(q, "data");
    cJSON* quotas = data ? cJSON_GetObjectItem(data, "quotas") : NULL;
    if (quotas && cJSON_IsArray(quotas)) {
        cJSON* qi;
        cJSON_ArrayForEach(qi, quotas) {
            const char* qc = json_get_str(qi, "quota_code", "");
            if (!qc[0]) continue;
            cJSON* det = get_package_detail(base_api, api_key, xdata_key, x_api_secret,
                                            id_token, qc);
            if (!det) continue;
            cJSON* dd = cJSON_GetObjectItem(det, "data");
            cJSON* pf = dd ? cJSON_GetObjectItem(dd, "package_family") : NULL;
            const char* fc = pf ? json_get_str(pf, "package_family_code", "") : "";
            int match = (strcmp(fc, family_code) == 0);
            cJSON_Delete(det);
            if (!match) continue;

            /* Sum benefits.remaining (DATA saja) */
            long long sum = 0;
            cJSON* benefits = cJSON_GetObjectItem(qi, "benefits");
            if (benefits && cJSON_IsArray(benefits)) {
                cJSON* b;
                cJSON_ArrayForEach(b, benefits) {
                    const char* dt = json_get_str(b, "data_type", "");
                    if (strcmp(dt, "DATA") != 0) continue;
                    cJSON* rem = cJSON_GetObjectItem(b, "remaining");
                    if (rem) sum += (long long)(cJSON_IsNumber(rem) ? rem->valuedouble
                                                                    : atof(rem->valuestring));
                }
            }
            if (total_rem < 0) total_rem = 0;
            total_rem += sum;
        }
    }
    cJSON_Delete(q);
    return total_rem;
}

static int ab_trigger_purchase(const char* base_api, const char* api_key,
                               const char* xdata_key, const char* x_api_secret,
                               const char* enc_field_key,
                               const char* id_token, const char* access_token,
                               cJSON* entry) {
    const char* opt_code   = json_get_str(entry, "option_code", "");
    const char* pm         = json_get_str(entry, "payment_method", "BALANCE");
    const char* nm         = json_get_str(entry, "name", "?");

    /* Fetch fresh detail untuk price/conf/pay_for */
    cJSON* det = get_package_detail(base_api, api_key, xdata_key, x_api_secret,
                                    id_token, opt_code);
    if (!det) { ab_log("[%s] gagal fetch package_detail", nm); return -1; }
    cJSON* d_data = cJSON_GetObjectItem(det, "data");
    cJSON* p_opt  = d_data ? cJSON_GetObjectItem(d_data, "package_option") : NULL;
    cJSON* p_fam  = d_data ? cJSON_GetObjectItem(d_data, "package_family") : NULL;
    if (!p_opt || !p_fam) { ab_log("[%s] detail malformed", nm); cJSON_Delete(det); return -1; }
    int price = cJSON_GetObjectItem(p_opt, "price") ? cJSON_GetObjectItem(p_opt, "price")->valueint : 0;
    const char* conf = json_get_str(d_data, "token_confirmation", "");
    const char* p_for = json_get_str(p_fam, "payment_for", "BUY_PACKAGE");

    const char* dec_code = NULL; int dec_price = 0;
    const char* dec_name = NULL; const char* dec_conf = NULL;
    cJSON* dec_det = NULL;

    if (strcmp(pm, "BALANCE_DECOY") == 0) {
        const char* dec_src = json_get_str(entry, "decoy_source", "");
        cJSON* de = cJSON_GetObjectItem(entry, "decoy");
        const char* dec_oc = de ? json_get_str(de, "option_code", "") : "";

        if (dec_oc[0]) {
            /* custom_inline: option_code sudah lengkap -> fetch detail langsung */
            dec_det = get_package_detail(base_api, api_key, xdata_key, x_api_secret,
                                          id_token, dec_oc);
        } else if (strcmp(dec_src, "default") == 0) {
            /* default: pakai fetch_decoy_package yang sudah ada di main.c
             * (pilih file decoy berdasarkan active_subs_type + resolve option). */
            dec_det = fetch_decoy_package(active_subs_type, base_api, api_key,
                                          xdata_key, x_api_secret, id_token);
        } else if (strcmp(dec_src, "custom_saved") == 0 && de) {
            /* custom_saved: resolve family+variant+order -> option_code -> detail */
            const char* fc = json_get_str(de, "family_code", "");
            const char* vc = json_get_str(de, "variant_code", "");
            int ord = cJSON_GetObjectItem(de, "order") ? cJSON_GetObjectItem(de, "order")->valueint : 0;
            int is_ent = cJSON_IsTrue(cJSON_GetObjectItem(de, "is_enterprise")) ? 1 : 0;
            const char* mig = json_get_str(de, "migration_type", "NONE");
            if (fc[0] && vc[0] && ord > 0) {
                cJSON* fam = do_family_bruteforce(base_api, api_key, xdata_key, x_api_secret,
                                                  id_token, fc, is_ent, mig);
                if (fam) {
                    char resolved[256] = {0};
                    cJSON* fdata = cJSON_GetObjectItem(fam, "data");
                    cJSON* variants = fdata ? cJSON_GetObjectItem(fdata, "package_variants") : NULL;
                    cJSON* v;
                    cJSON_ArrayForEach(v, variants) {
                        const char* vcode = json_get_str(v, "package_variant_code", "");
                        if (strcmp(vcode, vc) != 0) continue;
                        cJSON* opts = cJSON_GetObjectItem(v, "package_options");
                        cJSON* o;
                        cJSON_ArrayForEach(o, opts) {
                            cJSON* oord = cJSON_GetObjectItem(o, "order");
                            if (oord && oord->valueint == ord) {
                                snprintf(resolved, sizeof(resolved), "%s",
                                         json_get_str(o, "package_option_code", ""));
                                break;
                            }
                        }
                        break;
                    }
                    cJSON_Delete(fam);
                    if (resolved[0]) {
                        dec_det = get_package_detail(base_api, api_key, xdata_key, x_api_secret,
                                                     id_token, resolved);
                    }
                }
            }
        }

        if (!dec_det) {
            ab_log("[%s] decoy gagal resolve (source=%s); fallback BALANCE saja", nm,
                   dec_src[0] ? dec_src : "?");
            pm = "BALANCE";
        } else {
            cJSON* dd_data = cJSON_GetObjectItem(dec_det, "data");
            cJSON* dd_opt  = dd_data ? cJSON_GetObjectItem(dd_data, "package_option") : NULL;
            if (!dd_opt) {
                ab_log("[%s] decoy detail malformed; fallback BALANCE", nm);
                cJSON_Delete(dec_det); dec_det = NULL;
                pm = "BALANCE";
            } else {
                dec_code  = json_get_str(dd_opt, "package_option_code", "");
                if (!dec_code[0]) dec_code = NULL;
                dec_price = cJSON_GetObjectItem(dd_opt, "price") ? cJSON_GetObjectItem(dd_opt, "price")->valueint : 0;
                dec_name  = json_get_str(dd_opt, "name", "");
                dec_conf  = json_get_str(dd_data, "token_confirmation", "");
            }
        }
    }

    /* Pay-for: untuk decoy pakai emoji "🤑" (sama dengan menu Loop/manual);
     * untuk non-decoy pakai nilai dari package_family. Overwrite amount untuk
     * decoy = price + dec_price, untuk non-decoy = price. */
    int   overwrite_amount = dec_code ? (price + dec_price) : price;
    const char* pay_for    = dec_code ? "\xF0\x9F\xA4\x91" : p_for; /* 🤑 */
    int   conf_idx         = dec_code ? 1 : 0;

    ab_log("[%s] TRIGGER price=%d overwrite=%d method=%s", nm, price, overwrite_amount, pm);
    cJSON* res = execute_balance_purchase(base_api, api_key, xdata_key, x_api_secret,
                                          enc_field_key, id_token, access_token,
                                          opt_code, price, nm, conf,
                                          dec_code, dec_price, dec_name, dec_conf,
                                          pay_for, overwrite_amount, conf_idx);
    int ok = 0;
    if (res) {
        const char* st = json_get_str(res, "status", "");
        ok = (strcmp(st, "SUCCESS") == 0);
        ab_log("[%s] purchase response status=%s", nm, st[0] ? st : "(null)");
        cJSON_Delete(res);
    } else {
        ab_log("[%s] purchase response NULL", nm);
    }
    cJSON_Delete(det);
    if (dec_det) cJSON_Delete(dec_det);
    return ok ? 0 : -1;
}

int auto_buy_worker_main(void) {
    const char* base_ciam     = getenv("BASE_CIAM_URL");
    const char* base_api      = getenv("BASE_API_URL");
    const char* basic_auth    = getenv("BASIC_AUTH");
    const char* ua            = getenv("UA");
    const char* api_key       = getenv("API_KEY");
    const char* xdata_key     = getenv("XDATA_KEY");
    const char* x_api_secret  = getenv("X_API_BASE_SECRET");
    const char* enc_field_key = getenv("ENCRYPTED_FIELD_KEY");
    cJSON* tokens_arr         = g_tokens_arr;
    if (!base_api || !api_key || !xdata_key || !x_api_secret || !tokens_arr) {
        fprintf(stderr, "[auto-buy] env/tokens tidak lengkap, exit\n");
        return 1;
    }
    pid_t existing = ab_read_pid();
    if (existing > 0) {
        fprintf(stderr, "[auto-buy] worker lain masih aktif (PID=%d). Exit.\n", (int)existing);
        return 2;
    }
    if (ab_write_pid() != 0) {
        fprintf(stderr, "[auto-buy] gagal tulis %s (perlu root/permission)\n", AB_PID_PATH);
    }
    signal(SIGTERM, ab_on_term);
    signal(SIGINT,  ab_on_term);

    ab_log("worker started pid=%d", (int)getpid());
    int iter = 0;
    time_t next_refresh = 0;
    while (!g_ab_stop) {
        iter++;
        time_t now = time(NULL);
        if (now >= next_refresh) {
            /* Kalau id_tok kosong (refresh sebelumnya gagal atau belum pernah
             * auth) -> panggil authenticate_and_fetch_balance langsung. Fungsi
             * ini tidak butuh is_logged_in=1 dan akan men-set ulang is_logged_in
             * saat sukses, sehingga ensure_token_fresh bisa dipakai lagi. */
            if (!id_tok[0])
                authenticate_and_fetch_balance(tokens_arr, base_ciam, base_api, basic_auth, ua,
                                               api_key, xdata_key, x_api_secret);
            else
                ensure_token_fresh(tokens_arr, base_ciam, base_api, basic_auth, ua,
                                   api_key, xdata_key, x_api_secret);
            /* Kalau sukses maju 5 menit; kalau masih gagal retry 60 detik
             * supaya transient error (DNS/jaringan) tidak memblokir worker. */
            next_refresh = now + (id_tok[0] ? 300 : 60);
        }

        if (!id_tok[0]) {
            ab_log("no active id_token (auth gagal, retry 60s)");
            for (int w = 0; w < 60 && !g_ab_stop; w++) sleep(1);
            continue;
        }

        /* Per-entry next-check tracking, keyed by option_code supaya stabil
         * meskipun config di-reload (indeks bisa berubah kalau user hapus). */
        #define AB_MAX_TRACK 64
        static char  track_key[AB_MAX_TRACK][256];
        static time_t track_next[AB_MAX_TRACK];
        static int   track_init = 0;
        if (!track_init) {
            for (int i = 0; i < AB_MAX_TRACK; i++) { track_key[i][0] = 0; track_next[i] = 0; }
            track_init = 1;
        }

        cJSON* arr = ab_load();
        int n = cJSON_GetArraySize(arr);
        time_t min_next = now + 300;
        for (int i = 0; i < n; i++) {
            cJSON* e = cJSON_GetArrayItem(arr, i);
            if (!cJSON_IsTrue(cJSON_GetObjectItem(e, "enabled"))) continue;
            int interval = cJSON_GetObjectItem(e, "interval_sec") ? cJSON_GetObjectItem(e, "interval_sec")->valueint : 300;
            if (interval < 30) interval = 30;

            const char* name = json_get_str(e, "name", "?");
            const char* fc   = json_get_str(e, "family_code", "");
            const char* oc   = json_get_str(e, "option_code", "");
            int thr_mb = cJSON_GetObjectItem(e, "threshold_mb") ? cJSON_GetObjectItem(e, "threshold_mb")->valueint : 0;

            /* Lookup/alloc slot untuk entry ini */
            int slot = -1, empty = -1;
            for (int t = 0; t < AB_MAX_TRACK; t++) {
                if (track_key[t][0] && strcmp(track_key[t], oc) == 0) { slot = t; break; }
                if (!track_key[t][0] && empty < 0) empty = t;
            }
            if (slot < 0 && empty >= 0) {
                slot = empty;
                snprintf(track_key[slot], sizeof(track_key[slot]), "%s", oc);
                track_next[slot] = 0; /* cek pertama langsung */
            }
            if (slot < 0) {
                ab_log("[%s] track slot full (>%d entries), skip", name, AB_MAX_TRACK);
                continue;
            }

            if (now < track_next[slot]) {
                if (track_next[slot] < min_next) min_next = track_next[slot];
                continue;
            }

            long long rem = ab_check_remaining(base_api, api_key, xdata_key, x_api_secret,
                                               id_tok, fc);
            if (rem < 0) {
                ab_log("[%s] quota tidak ditemukan (belum pernah beli / belum aktif)", name);
            } else {
                double rem_mb = (double)rem / (1024.0 * 1024.0);
                ab_log("[%s] remaining=%.2f MB threshold=%d MB", name, rem_mb, thr_mb);
                if (rem_mb < (double)thr_mb) {
                    ab_trigger_purchase(base_api, api_key, xdata_key, x_api_secret,
                                        enc_field_key, id_tok, acc_tok, e);
                    ab_write_state(name, iter, "triggered");
                }
            }
            track_next[slot] = now + interval;
            if (track_next[slot] < min_next) min_next = track_next[slot];
        }
        cJSON_Delete(arr);
        ab_write_state(NULL, iter, "tick-done");

        /* sleep sampai entry paling awal jatuh tempo, minimal 1 detik, maks 300;
         * cek g_ab_stop tiap detik supaya stop responsif. */
        int sleep_sec = (int)(min_next - time(NULL));
        if (sleep_sec < 1)   sleep_sec = 1;
        if (sleep_sec > 300) sleep_sec = 300;
        for (int s = 0; s < sleep_sec && !g_ab_stop; s++) sleep(1);
    }
    ab_log("worker stopped pid=%d", (int)getpid());
    unlink(AB_PID_PATH);
    return 0;
}

/* ---------- menu ---------- */

static void ab_print_status(void) {
    pid_t p = ab_read_pid();
    if (p > 0) {
        /* Read state */
        size_t sz = 0;
        char* raw = file_read_all(AB_STATE_PATH, &sz);
        if (raw) {
            cJSON* st = cJSON_Parse(raw); free(raw);
            if (st) {
                time_t lt = (time_t)json_get_double(st, "last_tick", 0);
                int iters = (int)json_get_double(st, "iterations", 0);
                const char* ltn = json_get_str(st, "last_trigger_name", "-");
                char stamp[32] = "-";
                if (lt > 0) { struct tm tmv; localtime_r(&lt, &tmv); strftime(stamp, sizeof(stamp), "%H:%M:%S", &tmv); }
                printf(" Status worker: RUNNING (pid=%d, iter=%d, last_tick=%s, last_trigger=%s)\n",
                       (int)p, iters, stamp, ltn);
                cJSON_Delete(st);
                return;
            }
        }
        printf(" Status worker: RUNNING (pid=%d, no state file)\n", (int)p);
    } else {
        printf(" Status worker: STOPPED\n");
    }
}

static void ab_tail_log(int n) {
    FILE* f = fopen(AB_LOG_PATH, "r");
    if (!f) { printf(" (log kosong)\n"); return; }
    /* naive tail: read all, keep last n lines */
    fseek(f, 0, SEEK_END); long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        if (sz == 0) printf(" (log kosong)\n");
        return;
    }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = 0; fclose(f);
    sz = (long)got;
    /* count newlines, find nth from end */
    int lines = 0;
    for (long i = sz - 1; i >= 0; i--) {
        if (buf[i] == '\n') { lines++; if (lines > n) { printf("%s", buf + i + 1); free(buf); return; } }
    }
    printf("%s", buf);
    free(buf);
}

void auto_buy_menu(const char* base_api, const char* api_key,
                   const char* xdata_key, const char* x_api_secret,
                   const char* enc_field_key, const char* id_token) {
    (void)enc_field_key;
    while (1) {
        ab_clear();
        printf("=======================================================\n");
        printf("         AUTO BUY\n");
        printf("=======================================================\n");
        ab_print_status();

        cJSON* arr = ab_load();
        int n = cJSON_GetArraySize(arr);
        if (n == 0) {
            printf(" (belum ada entry)\n");
        } else {
            for (int i = 0; i < n; i++) {
                cJSON* e = cJSON_GetArrayItem(arr, i);
                int enabled = cJSON_IsTrue(cJSON_GetObjectItem(e, "enabled"));
                const char* pm = json_get_str(e, "payment_method", "BALANCE");
                printf("%2d. %s  interval=%ds  threshold=%dMB  method=%s  [%s]\n",
                       i + 1,
                       json_get_str(e, "name", "?"),
                       cJSON_GetObjectItem(e, "interval_sec") ? cJSON_GetObjectItem(e, "interval_sec")->valueint : 0,
                       cJSON_GetObjectItem(e, "threshold_mb") ? cJSON_GetObjectItem(e, "threshold_mb")->valueint : 0,
                       pm,
                       enabled ? "ON" : "OFF");
            }
        }

        printf("-------------------------------------------------------\n");
        printf("Perintah:\n");
        printf("  add             Tambah entry baru\n");
        printf("  del <nomor>     Hapus entry\n");
        printf("  toggle <nomor>  Aktifkan/nonaktifkan entry\n");
        printf("  start           Jalankan worker di foreground (Ctrl+C untuk stop)\n");
        printf("  stop            Matikan worker yang jalan background\n");
        printf("  log             Tampilkan 20 baris log terakhir\n");
        printf("  00              Kembali\n");
        printf("Pilihan: ");
        fflush(stdout);

        char cmd[64]; if (!fgets(cmd, sizeof(cmd), stdin)) { cJSON_Delete(arr); return; }
        cmd[strcspn(cmd, "\n")] = 0;

        int num_arg;
        if (strcmp(cmd, "00") == 0 || strcmp(cmd, "99") == 0) { cJSON_Delete(arr); return; }
        else if (strcasecmp(cmd, "add") == 0 || strcasecmp(cmd, "a") == 0) {
            cJSON_Delete(arr);
            ab_add(base_api, api_key, xdata_key, x_api_secret, id_token);
        }
        else if ((strncasecmp(cmd, "del ", 4) == 0 && (num_arg = atoi(cmd + 4)) >= 0) ||
                 (strncasecmp(cmd, "d ",   2) == 0 && (num_arg = atoi(cmd + 2)) >= 0)) {
            if (num_arg < 1 || num_arg > n) { printf("[-] Nomor di luar range.\n"); cJSON_Delete(arr); pause_enter(); continue; }
            cJSON* e = cJSON_GetArrayItem(arr, num_arg - 1);
            char* nm = strdup(json_get_str(e, "name", "?"));
            cJSON_DeleteItemFromArray(arr, num_arg - 1);
            if (ab_save(arr) == 0) printf("[+] Dihapus: %s\n", nm ? nm : "?");
            else                   printf("[-] Gagal simpan.\n");
            free(nm);
            cJSON_Delete(arr); pause_enter();
        }
        else if ((strncasecmp(cmd, "toggle ", 7) == 0 && (num_arg = atoi(cmd + 7)) >= 0) ||
                 (strncasecmp(cmd, "t ",      2) == 0 && (num_arg = atoi(cmd + 2)) >= 0)) {
            if (num_arg < 1 || num_arg > n) { printf("[-] Nomor di luar range.\n"); cJSON_Delete(arr); pause_enter(); continue; }
            cJSON* e = cJSON_GetArrayItem(arr, num_arg - 1);
            int enabled = cJSON_IsTrue(cJSON_GetObjectItem(e, "enabled"));
            cJSON_ReplaceItemInObject(e, "enabled", cJSON_CreateBool(!enabled));
            if (ab_save(arr) == 0) printf("[+] Entry %d: %s\n", num_arg, !enabled ? "ON" : "OFF");
            else                   printf("[-] Gagal simpan.\n");
            cJSON_Delete(arr); pause_enter();
        }
        else if (strcmp(cmd, "start") == 0) {
            cJSON_Delete(arr);
            pid_t p = ab_read_pid();
            if (p > 0) { printf("[-] Worker sudah jalan (pid=%d). Pakai 'stop' dulu.\n", (int)p); pause_enter(); continue; }
            printf("[*] Menjalankan worker foreground. Tekan Ctrl+C untuk berhenti...\n");
            auto_buy_worker_main();
            /* Restore default SIGINT/SIGTERM supaya Ctrl+C di menu interaktif
             * berjalan seperti biasa (handler ab_on_term dipasang oleh worker). */
            signal(SIGINT,  SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            g_ab_stop = 0;
            printf("[+] Worker berhenti.\n");
            pause_enter();
        }
        else if (strcmp(cmd, "stop") == 0) {
            cJSON_Delete(arr);
            pid_t p = ab_read_pid();
            if (p <= 0) { printf("[-] Worker tidak sedang jalan.\n"); pause_enter(); continue; }
            if (kill(p, SIGTERM) == 0) {
                printf("[+] SIGTERM dikirim ke pid=%d.\n", (int)p);
            } else {
                printf("[-] Gagal kill pid=%d: %s\n", (int)p, strerror(errno));
            }
            pause_enter();
        }
        else if (strcmp(cmd, "log") == 0) {
            cJSON_Delete(arr);
            printf("\n--- tail 20 baris %s ---\n", AB_LOG_PATH);
            ab_tail_log(20);
            printf("--- end ---\n");
            pause_enter();
        }
        else {
            cJSON_Delete(arr);
        }
    }
}

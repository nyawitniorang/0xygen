#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "../include/cJSON.h"
#include "../include/menu/features.h"
#include "../include/menu/auto_buy.h"
#include "../include/client/circle.h"
#include "../include/client/famplan.h"
#include "../include/client/store.h"
#include "../include/client/redeem.h"
#include "../include/client/transfer.h"
#include "../include/client/engsel.h"
#include "../include/menu/purchase_flow.h"
#include "../include/service/crypto_aes.h"
#include "../include/util/json_util.h"
#include "../include/util/file_util.h"
#include "../include/util/phone.h"

/* Tipe kartu aktif global (didefinisikan di main.c). */
extern char active_subs_type[32];

/* ---------------------------------------------------------------------------
 * Helper utilities
 * ------------------------------------------------------------------------- */

#define W 55

static void fx_clear(void) { printf("\033[H\033[J"); fflush(stdout); }

/* Cocokkan kata kunci command: `add` atau alias `a`. Return 1 kalau cocok. */
static int cmd_eq(const char* cmd, const char* word, const char* alias) {
    if (strcasecmp(cmd, word) == 0) return 1;
    if (alias && strcasecmp(cmd, alias) == 0) return 1;
    return 0;
}

/* Cocokkan command yang diikuti angka: `del 3` atau `d 3`. Return angka (>=1)
 * kalau cocok, 0 kalau kata cocok tapi argumen invalid, -1 kalau tidak cocok. */
static int cmd_num(const char* cmd, const char* word, const char* alias) {
    size_t wlen = strlen(word);
    size_t alen = alias ? strlen(alias) : 0;
    const char* p = NULL;
    if (strncasecmp(cmd, word, wlen) == 0 && (cmd[wlen] == ' ' || cmd[wlen] == '\t'))
        p = cmd + wlen;
    else if (alias && strncasecmp(cmd, alias, alen) == 0 && (cmd[alen] == ' ' || cmd[alen] == '\t'))
        p = cmd + alen;
    else
        return -1;
    while (*p == ' ' || *p == '\t') p++;
    int v = atoi(p);
    return v > 0 ? v : 0;
}

static void flush_line(void) {
    int c; while ((c = getchar()) != '\n' && c != EOF);
}

static void pause_enter(void) {
    printf("\nTekan Enter untuk melanjutkan...");
    fflush(stdout); flush_line();
}

static void read_line(const char* prompt, char* buf, size_t n) {
    printf("%s", prompt); fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
}

static int read_yn(const char* prompt) {
    char b[8]; read_line(prompt, b, sizeof(b));
    return (b[0] == 'y' || b[0] == 'Y');
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

static void format_quota(long long bytes, char* out, size_t n) {
    const long long GB = 1024LL * 1024 * 1024;
    const long long MB = 1024LL * 1024;
    const long long KB = 1024LL;
    if (bytes >= GB) snprintf(out, n, "%.2f GB", (double)bytes / GB);
    else if (bytes >= MB) snprintf(out, n, "%.2f MB", (double)bytes / MB);
    else if (bytes >= KB) snprintf(out, n, "%.2f KB", (double)bytes / KB);
    else snprintf(out, n, "%lld B", bytes);
}

static void format_ts(double ts, char* out, size_t n) {
    if (ts <= 0) { snprintf(out, n, "N/A"); return; }
    time_t t = (time_t)ts;
    struct tm lt; localtime_r(&t, &lt);
    strftime(out, n, "%Y-%m-%d", &lt);
}

static void rule(char ch) {
    for (int i = 0; i < W; i++) putchar(ch);
    putchar('\n');
}

static void dump_json(const char* label, cJSON* res) {
    printf("\n=== %s ===\n", label);
    if (!res) { printf("[-] Tidak ada response.\n"); return; }
    char* out = cJSON_Print(res); if (out) { printf("%s\n", out); free(out); }
}

/* ---------------------------------------------------------------------------
 * CIRCLE
 * ------------------------------------------------------------------------- */

static void circle_bonus_menu(const char* base, const char* api_key,
                              const char* xdata_key, const char* sec,
                              const char* id_token,
                              const char* parent_subs_id, const char* family_id) {
    while (1) {
        cJSON* res = circle_get_bonus_list(base, api_key, xdata_key, sec,
                                           id_token, parent_subs_id, family_id);
        if (!json_status_is_success(res)) {
            printf("[-] Gagal ambil bonus list.\n"); dump_json("bonus", res);
            cJSON_Delete(res); pause_enter(); return;
        }
        cJSON* data = cJSON_GetObjectItemCaseSensitive(res, "data");
        cJSON* bonuses = data ? cJSON_GetObjectItemCaseSensitive(data, "bonuses") : NULL;
        int count = cJSON_IsArray(bonuses) ? cJSON_GetArraySize(bonuses) : 0;

        rule('='); printf("%*s\n", (int)((W + 18) / 2), "Circle Bonus List"); rule('=');
        if (count == 0) {
            printf("Tidak ada bonus.\n");
            cJSON_Delete(res); pause_enter(); return;
        }

        for (int i = 0; i < count; i++) {
            cJSON* b = cJSON_GetArrayItem(bonuses, i);
            printf("%d. %s | Type: %s\n", i + 1,
                   json_get_str(b, "name", "N/A"),
                   json_get_str(b, "bonus_type", "N/A"));
            printf("   Action: %s | Param: %s\n",
                   json_get_str(b, "action_type", "N/A"),
                   json_get_str(b, "action_param", "N/A"));
        }
        rule('-'); printf("00. Kembali\nPilihan: "); fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) { cJSON_Delete(res); return; }
        ch[strcspn(ch, "\n")] = 0;
        cJSON_Delete(res);
        if (strcmp(ch, "00") == 0) return;
    }
}

static void circle_creation(const char* base, const char* api_key,
                            const char* xdata_key, const char* sec,
                            const char* enc_field_key, const char* id_token,
                            const char* access_token) {
    char pname[64], gname[64], mmsisdn[32], mname[64];
    printf("\n=== Create Circle ===\n");
    read_line("Nama Anda (Parent): ", pname, sizeof(pname));
    read_line("Nama Circle: ", gname, sizeof(gname));
    read_msisdn("MSISDN anggota awal (08/62xxxx): ", mmsisdn, sizeof(mmsisdn));
    read_line("Nama anggota awal: ", mname, sizeof(mname));
    cJSON* res = circle_create_group(base, api_key, xdata_key, sec, enc_field_key,
                                     id_token, access_token, pname, gname, mmsisdn, mname);
    dump_json("create_circle", res);
    cJSON_Delete(res);
    pause_enter();
}

static void circle_info_menu(const char* base, const char* api_key,
                             const char* xdata_key, const char* sec,
                             const char* enc_field_key, const char* id_token,
                             const char* access_token, const char* my_msisdn) {
    while (1) {
        cJSON* group_res = circle_get_group_data(base, api_key, xdata_key, sec, id_token);
        if (!json_status_is_success(group_res)) {
            printf("[-] Gagal ambil data circle.\n"); cJSON_Delete(group_res);
            pause_enter(); return;
        }
        cJSON* gd = cJSON_GetObjectItemCaseSensitive(group_res, "data");
        const char* group_id = json_get_str(gd, "group_id", "");
        if (!group_id[0]) {
            printf("\nAnda belum tergabung dalam Circle.\n");
            if (read_yn("Buat Circle baru? (y/n): ")) {
                cJSON_Delete(group_res);
                circle_creation(base, api_key, xdata_key, sec, enc_field_key,
                                id_token, access_token);
                continue;
            }
            cJSON_Delete(group_res); pause_enter(); return;
        }
        const char* gstatus = json_get_str(gd, "group_status", "N/A");
        const char* gname = json_get_str(gd, "group_name", "N/A");
        const char* owner = json_get_str(gd, "owner_name", "N/A");
        if (strcmp(gstatus, "BLOCKED") == 0) {
            printf("Circle diblokir.\n"); cJSON_Delete(group_res); pause_enter(); return;
        }

        cJSON* members_res = circle_get_group_members(base, api_key, xdata_key, sec,
                                                      id_token, group_id);
        if (!json_status_is_success(members_res)) {
            printf("[-] Gagal ambil anggota.\n"); cJSON_Delete(group_res);
            cJSON_Delete(members_res); pause_enter(); return;
        }
        cJSON* md = cJSON_GetObjectItemCaseSensitive(members_res, "data");
        cJSON* members = md ? cJSON_GetObjectItemCaseSensitive(md, "members") : NULL;
        int mcount = cJSON_IsArray(members) ? cJSON_GetArraySize(members) : 0;

        char parent_mid[128] = "", parent_subs[128] = "";
        for (int i = 0; i < mcount; i++) {
            cJSON* m = cJSON_GetArrayItem(members, i);
            if (strcmp(json_get_str(m, "member_role", ""), "PARENT") == 0) {
                snprintf(parent_mid, sizeof(parent_mid), "%s", json_get_str(m, "member_id", ""));
                snprintf(parent_subs, sizeof(parent_subs), "%s",
                         json_get_str(m, "subscriber_number", ""));
                break;
            }
        }

        cJSON* pkg = md ? cJSON_GetObjectItemCaseSensitive(md, "package") : NULL;
        const char* pkgname = json_get_str(pkg, "name", "N/A");
        cJSON* benefit = pkg ? cJSON_GetObjectItemCaseSensitive(pkg, "benefit") : NULL;
        long long alloc = (long long)json_get_double(benefit, "allocation", 0);
        long long remain = (long long)json_get_double(benefit, "remaining", 0);
        char alloc_s[32], remain_s[32];
        format_quota(alloc, alloc_s, sizeof(alloc_s));
        format_quota(remain, remain_s, sizeof(remain_s));

        cJSON* spend_res = circle_spending_tracker(base, api_key, xdata_key, sec,
                                                   id_token, parent_subs, group_id);
        long long spend = 0, target = 0;
        if (json_status_is_success(spend_res)) {
            cJSON* sd = cJSON_GetObjectItemCaseSensitive(spend_res, "data");
            spend = (long long)json_get_double(sd, "spend", 0);
            target = (long long)json_get_double(sd, "target", 0);
        }

        printf("\n"); rule('=');
        printf("Circle: %s (%s)\n", gname, gstatus);
        printf("Owner : %s\n", owner);
        rule('-');
        printf("Package: %s | %s / %s\n", pkgname, remain_s, alloc_s);
        printf("Spending: Rp%lld / Rp%lld\n", spend, target);
        rule('=');
        printf("Members:\n");
        for (int i = 0; i < mcount; i++) {
            cJSON* m = cJSON_GetArrayItem(members, i);
            const char* enc_msisdn = json_get_str(m, "msisdn", "");
            char* plain = decrypt_circle_msisdn(enc_msisdn, enc_field_key);
            const char* show_msisdn = (plain && plain[0]) ? plain : "<No Number>";
            const char* mname = json_get_str(m, "member_name", "N/A");
            const char* role = json_get_str(m, "member_role", "N/A");
            const char* mstatus = json_get_str(m, "status", "N/A");
            const char* slot = json_get_str(m, "slot_type", "N/A");
            double jts = json_get_double(m, "join_date", 0);
            long long malloc_b = (long long)json_get_double(m, "allocation", 0);
            long long mrem_b = (long long)json_get_double(m, "remaining", 0);
            char mallocs[32], muses[32], jdate[32];
            format_quota(malloc_b, mallocs, sizeof(mallocs));
            format_quota(malloc_b - mrem_b, muses, sizeof(muses));
            format_ts(jts, jdate, sizeof(jdate));
            const char* me = (plain && my_msisdn && strcmp(plain, my_msisdn) == 0) ? " (You)" : "";
            printf("%d. %s (%s) | %s%s\n", i + 1, show_msisdn, mname,
                   strcmp(role, "PARENT") == 0 ? "Parent" : "Member", me);
            printf("   Joined: %s | Slot: %s | Status: %s\n", jdate, slot, mstatus);
            printf("   Usage: %s / %s\n", muses, mallocs);
            rule('-');
            free(plain);
        }
        rule('-');
        printf("Options:\n"
               "1. Invite Member\n"
               "del <N>. Remove member di posisi N\n"
               "acc <N>. Accept invitation member di posisi N\n"
               "2. Bonus List\n"
               "00. Kembali\n"
               "Pilihan: ");
        fflush(stdout);
        char ch[64]; if (!fgets(ch, sizeof(ch), stdin)) { ch[0] = 0; }
        ch[strcspn(ch, "\n")] = 0;

        if (strcmp(ch, "00") == 0) {
            cJSON_Delete(group_res); cJSON_Delete(members_res); cJSON_Delete(spend_res);
            return;
        }
        else if (strcmp(ch, "1") == 0) {
            char inv_m[32], inv_n[64];
            read_msisdn("MSISDN (08/62xxxx): ", inv_m, sizeof(inv_m));
            cJSON* v = circle_validate_member(base, api_key, xdata_key, sec,
                                              enc_field_key, id_token, inv_m);
            if (json_status_is_success(v)) {
                cJSON* vd = cJSON_GetObjectItemCaseSensitive(v, "data");
                const char* rcode = json_get_str(vd, "response_code", "");
                if (strcmp(rcode, "200-2001") != 0) {
                    printf("[-] Tidak bisa invite %s: %s\n",
                           inv_m, json_get_str(vd, "message", "Unknown"));
                    cJSON_Delete(v); pause_enter(); continue;
                }
            }
            cJSON_Delete(v);
            read_line("Nama anggota: ", inv_n, sizeof(inv_n));
            cJSON* r = circle_invite_member(base, api_key, xdata_key, sec,
                                            enc_field_key, id_token, access_token,
                                            inv_m, inv_n, group_id, parent_mid);
            dump_json("invite", r); cJSON_Delete(r); pause_enter();
        }
        else if (strncmp(ch, "del ", 4) == 0) {
            int n = atoi(ch + 4);
            if (n < 1 || n > mcount) { printf("[-] Index salah.\n"); pause_enter(); continue; }
            cJSON* m = cJSON_GetArrayItem(members, n - 1);
            if (strcmp(json_get_str(m, "member_role", ""), "PARENT") == 0) {
                printf("[-] Tidak bisa hapus parent.\n"); pause_enter(); continue;
            }
            int is_last = (mcount == 2);
            if (is_last) { printf("[-] Tidak bisa hapus member terakhir.\n"); pause_enter(); continue; }
            char* plain = decrypt_circle_msisdn(json_get_str(m, "msisdn", ""), enc_field_key);
            printf("Yakin hapus %s? (y/n): ", plain ? plain : "?");
            int ok = read_yn("");
            if (!ok) { free(plain); continue; }
            cJSON* r = circle_remove_member(base, api_key, xdata_key, sec, id_token,
                                            json_get_str(m, "member_id", ""),
                                            group_id, parent_mid, is_last);
            dump_json("remove", r); cJSON_Delete(r); free(plain); pause_enter();
        }
        else if (strncmp(ch, "acc ", 4) == 0) {
            int n = atoi(ch + 4);
            if (n < 1 || n > mcount) { printf("[-] Index salah.\n"); pause_enter(); continue; }
            cJSON* m = cJSON_GetArrayItem(members, n - 1);
            if (strcmp(json_get_str(m, "status", ""), "INVITED") != 0) {
                printf("[-] Member tidak dalam status INVITED.\n"); pause_enter(); continue;
            }
            cJSON* r = circle_accept_invitation(base, api_key, xdata_key, sec,
                                                id_token, access_token, group_id,
                                                json_get_str(m, "member_id", ""));
            dump_json("accept", r); cJSON_Delete(r); pause_enter();
        }
        else if (strcmp(ch, "2") == 0) {
            cJSON_Delete(group_res); cJSON_Delete(members_res); cJSON_Delete(spend_res);
            circle_bonus_menu(base, api_key, xdata_key, sec, id_token, parent_subs, group_id);
            continue;
        }
        cJSON_Delete(group_res); cJSON_Delete(members_res); cJSON_Delete(spend_res);
    }
}

/* ---------------------------------------------------------------------------
 * FAMILY PLAN
 * ------------------------------------------------------------------------- */

static void famplan_menu(const char* base, const char* api_key,
                         const char* xdata_key, const char* sec,
                         const char* id_token) {
    while (1) {
        cJSON* res = famplan_get_member_info(base, api_key, xdata_key, sec, id_token);
        cJSON* data = res ? cJSON_GetObjectItemCaseSensitive(res, "data") : NULL;
        if (!data) {
            printf("[-] Gagal ambil data Family Plan.\n");
            cJSON_Delete(res); pause_enter(); return;
        }
        cJSON* mi = cJSON_GetObjectItemCaseSensitive(data, "member_info");
        const char* plan_type = json_get_str(mi, "plan_type", "");
        if (!plan_type[0]) {
            printf("Anda bukan organizer Family Plan.\n");
            cJSON_Delete(res); pause_enter(); return;
        }
        const char* parent = json_get_str(mi, "parent_msisdn", "");
        long long total = (long long)json_get_double(mi, "total_quota", 0);
        long long remaining = (long long)json_get_double(mi, "remaining_quota", 0);
        double endts = json_get_double(mi, "end_date", 0);
        char totalb[32], remb[32], endd[32];
        format_quota(total, totalb, sizeof(totalb));
        format_quota(remaining, remb, sizeof(remb));
        format_ts(endts, endd, sizeof(endd));

        cJSON* members = cJSON_GetObjectItemCaseSensitive(mi, "members");
        int mcount = cJSON_IsArray(members) ? cJSON_GetArraySize(members) : 0;
        int empty = 0;
        for (int i = 0; i < mcount; i++) {
            cJSON* m = cJSON_GetArrayItem(members, i);
            if (!json_get_str(m, "msisdn", "")[0]) empty++;
        }

        printf("\n"); rule('-');
        printf("Plan: %s | Parent: %s\n", plan_type, parent);
        printf("Shared: %s / %s | Exp: %s\n", remb, totalb, endd);
        rule('-');
        printf("Members: %d/%d\n", mcount - empty, mcount);
        for (int i = 0; i < mcount; i++) {
            cJSON* m = cJSON_GetArrayItem(members, i);
            const char* ms = json_get_str(m, "msisdn", "");
            const char* alias = json_get_str(m, "alias", "N/A");
            const char* mtype = json_get_str(m, "member_type", "N/A");
            int ac = json_get_int(m, "add_chances", 0);
            int tac = json_get_int(m, "total_add_chances", 0);
            cJSON* us = cJSON_GetObjectItemCaseSensitive(m, "usage");
            long long qa = (long long)json_get_double(us, "quota_allocated", 0);
            long long qu = (long long)json_get_double(us, "quota_used", 0);
            double qexp = json_get_double(us, "quota_expired_at", 0);
            char qas[32], qus[32], edat[32];
            format_quota(qa, qas, sizeof(qas));
            format_quota(qu, qus, sizeof(qus));
            format_ts(qexp, edat, sizeof(edat));
            rule('-');
            printf("%d. %s (%s) | %s | Chances: %d/%d\n", i + 1,
                   ms[0] ? ms : "<Empty Slot>", alias, mtype, ac, tac);
            printf("   Usage: %s / %s | Exp: %s\n", qus, qas, edat);
        }
        rule('-');
        printf("Options:\n"
               "1. Change Member (ganti nomor slot kosong)\n"
               "limit <slot> <MB>. Set quota limit\n"
               "del <slot>. Remove member\n"
               "00. Kembali\n"
               "Pilihan: ");
        fflush(stdout);
        char ch[64]; if (!fgets(ch, sizeof(ch), stdin)) { ch[0] = 0; }
        ch[strcspn(ch, "\n")] = 0;

        if (strcmp(ch, "00") == 0) { cJSON_Delete(res); return; }
        else if (strcmp(ch, "1") == 0) {
            char sb[8], tm[32], pa[64], ca[64];
            read_line("Slot: ", sb, sizeof(sb));
            read_msisdn("MSISDN baru (08/62xxxx): ", tm, sizeof(tm));
            read_line("Alias Anda: ", pa, sizeof(pa));
            read_line("Alias anggota: ", ca, sizeof(ca));
            int slot_idx = atoi(sb);
            if (slot_idx < 1 || slot_idx > mcount) {
                printf("[-] Slot invalid.\n"); cJSON_Delete(res); pause_enter(); continue;
            }
            cJSON* m = cJSON_GetArrayItem(members, slot_idx - 1);
            if (json_get_str(m, "msisdn", "")[0]) {
                printf("[-] Slot tidak kosong.\n"); cJSON_Delete(res); pause_enter(); continue;
            }
            const char* fmid = json_get_str(m, "family_member_id", "");
            int slot_id = json_get_int(m, "slot_id", 0);

            cJSON* vres = famplan_validate_msisdn(base, api_key, xdata_key, sec,
                                                  id_token, tm);
            if (!json_status_is_success(vres)) {
                printf("[-] Nomor tidak valid.\n");
                dump_json("validate", vres); cJSON_Delete(vres);
                cJSON_Delete(res); pause_enter(); continue;
            }
            cJSON_Delete(vres);
            cJSON* r = famplan_change_member(base, api_key, xdata_key, sec, id_token,
                                             pa, ca, slot_id, fmid, tm);
            dump_json("change_member", r); cJSON_Delete(r); pause_enter();
        }
        else if (strncmp(ch, "limit ", 6) == 0) {
            int slot = 0, mb = 0;
            if (sscanf(ch + 6, "%d %d", &slot, &mb) != 2 || slot < 1 || slot > mcount) {
                printf("[-] Format: limit <slot> <MB>\n"); cJSON_Delete(res); pause_enter(); continue;
            }
            cJSON* m = cJSON_GetArrayItem(members, slot - 1);
            const char* fmid = json_get_str(m, "family_member_id", "");
            cJSON* us = cJSON_GetObjectItemCaseSensitive(m, "usage");
            long long orig = (long long)json_get_double(us, "quota_allocated", 0);
            long long newbytes = (long long)mb * 1024LL * 1024LL;
            cJSON* r = famplan_set_quota_limit(base, api_key, xdata_key, sec, id_token,
                                               orig, newbytes, fmid);
            dump_json("set_quota_limit", r); cJSON_Delete(r); pause_enter();
        }
        else if (strncmp(ch, "del ", 4) == 0) {
            int slot = atoi(ch + 4);
            if (slot < 1 || slot > mcount) {
                printf("[-] Slot invalid.\n"); cJSON_Delete(res); pause_enter(); continue;
            }
            cJSON* m = cJSON_GetArrayItem(members, slot - 1);
            const char* fmid = json_get_str(m, "family_member_id", "");
            if (!read_yn("Yakin hapus? (y/n): ")) { cJSON_Delete(res); continue; }
            cJSON* r = famplan_remove_member(base, api_key, xdata_key, sec, id_token, fmid);
            dump_json("remove_member", r); cJSON_Delete(r); pause_enter();
        }
        cJSON_Delete(res);
    }
}

/* ---------------------------------------------------------------------------
 * STORE
 * ------------------------------------------------------------------------- */

__attribute__((unused))
static void store_menu(const char* base, const char* api_key,
                       const char* xdata_key, const char* sec,
                       const char* id_token) {
    while (1) {
        printf("\n=== STORE ===\n"
               "1. Family list (PREPAID)\n"
               "2. Search paket (v9, PREPAID)\n"
               "3. Segments\n"
               "4. Notifications\n"
               "00. Kembali\n"
               "Pilihan: ");
        fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) return;
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0) return;
        else if (strcmp(ch, "1") == 0) {
            cJSON* r = store_get_family_list(base, api_key, xdata_key, sec, id_token, "PREPAID", 0);
            dump_json("family-list", r); cJSON_Delete(r); pause_enter();
        }
        else if (strcmp(ch, "2") == 0) {
            cJSON* r = store_get_packages(base, api_key, xdata_key, sec, id_token, "PREPAID", 0);
            dump_json("search v9", r); cJSON_Delete(r); pause_enter();
        }
        else if (strcmp(ch, "3") == 0) {
            cJSON* r = store_get_segments(base, api_key, xdata_key, sec, id_token, 0);
            dump_json("segments", r); cJSON_Delete(r); pause_enter();
        }
        else if (strcmp(ch, "4") == 0) {
            cJSON* r = store_get_notifications(base, api_key, xdata_key, sec, id_token);
            dump_json("notifications", r); cJSON_Delete(r); pause_enter();
        }
    }
}

/* ---------------------------------------------------------------------------
 * REDEEM (bounty / loyalty / allotment)
 * ------------------------------------------------------------------------- */

static void redeem_bounty_flow(const char* base, const char* api_key,
                               const char* xdata_key, const char* sec,
                               const char* enc_field_key,
                               const char* id_token, const char* access_token) {
    cJSON* red = store_get_redeemables(base, api_key, xdata_key, sec, id_token, 0);
    if (!json_status_is_success(red)) {
        printf("[-] Gagal ambil redeemables.\n"); dump_json("redeemables", red);
        cJSON_Delete(red); pause_enter(); return;
    }
    cJSON* d = cJSON_GetObjectItemCaseSensitive(red, "data");
    cJSON* items = d ? cJSON_GetObjectItemCaseSensitive(d, "redeemables") : NULL;
    if (!items) items = d ? cJSON_GetObjectItemCaseSensitive(d, "items") : NULL;
    int n = cJSON_IsArray(items) ? cJSON_GetArraySize(items) : 0;
    if (n == 0) {
        printf("Tidak ada item redeem.\n"); dump_json("raw", red);
        cJSON_Delete(red); pause_enter(); return;
    }
    rule('='); printf("Redeemable (Bounty)\n"); rule('-');
    for (int i = 0; i < n; i++) {
        cJSON* it = cJSON_GetArrayItem(items, i);
        printf("%d. %s | code=%s | price=%d\n", i + 1,
               json_get_str(it, "name", "?"),
               json_get_str(it, "item_code", json_get_str(it, "code", "?")),
               json_get_int(it, "price", 0));
    }
    rule('-'); printf("Pilih (1-%d, 00 kembali): ", n); fflush(stdout);
    char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) { cJSON_Delete(red); return; }
    ch[strcspn(ch, "\n")] = 0;
    if (strcmp(ch, "00") == 0) { cJSON_Delete(red); return; }
    int idx = atoi(ch);
    if (idx < 1 || idx > n) { cJSON_Delete(red); return; }
    cJSON* it = cJSON_GetArrayItem(items, idx - 1);
    const char* code = json_get_str(it, "item_code", json_get_str(it, "code", ""));
    const char* nm = json_get_str(it, "name", "");
    int price = json_get_int(it, "price", 0);
    const char* token_conf = json_get_str(it, "token_confirmation",
                                          json_get_str(it, "confirmation_token", ""));
    long ts_sign = (long)time(NULL);

    cJSON* r = redeem_settlement_bounty(base, api_key, xdata_key, sec, enc_field_key,
                                        id_token, access_token, token_conf, ts_sign,
                                        code, price, nm);
    dump_json("settlement_bounty", r); cJSON_Delete(r);
    cJSON_Delete(red);
    pause_enter();
}

static void redeem_loyalty_flow(const char* base, const char* api_key,
                                const char* xdata_key, const char* sec,
                                const char* id_token) {
    char code[128], tokc[256], pb[16];
    read_line("Item code: ", code, sizeof(code));
    read_line("Token confirmation: ", tokc, sizeof(tokc));
    read_line("Points: ", pb, sizeof(pb));
    int points = atoi(pb);
    long ts = (long)time(NULL);
    cJSON* r = redeem_settlement_loyalty(base, api_key, xdata_key, sec, id_token,
                                         tokc, ts, code, points);
    dump_json("settlement_loyalty", r); cJSON_Delete(r); pause_enter();
}

static void redeem_allotment_flow(const char* base, const char* api_key,
                                  const char* xdata_key, const char* sec,
                                  const char* id_token) {
    char code[128], nm[128], dst[32], tokc[256];
    read_line("Item code: ", code, sizeof(code));
    read_line("Item name: ", nm, sizeof(nm));
    read_msisdn("Destination MSISDN (08/62xxxx): ", dst, sizeof(dst));
    read_line("Token confirmation: ", tokc, sizeof(tokc));
    long ts = (long)time(NULL);
    cJSON* r = redeem_bounty_allotment(base, api_key, xdata_key, sec, id_token,
                                       ts, dst, nm, code, tokc);
    dump_json("bounty_allotment", r); cJSON_Delete(r); pause_enter();
}

__attribute__((unused))
static void redeem_menu(const char* base, const char* api_key,
                        const char* xdata_key, const char* sec,
                        const char* enc_field_key,
                        const char* id_token, const char* access_token) {
    while (1) {
        printf("\n=== REDEEM ===\n"
               "1. Bounty exchange (dari daftar redeemables)\n"
               "2. Loyalty tiering exchange (manual)\n"
               "3. Bounty allotment ke MSISDN lain (manual)\n"
               "00. Kembali\n"
               "Pilihan: ");
        fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) return;
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0) return;
        else if (strcmp(ch, "1") == 0)
            redeem_bounty_flow(base, api_key, xdata_key, sec, enc_field_key, id_token, access_token);
        else if (strcmp(ch, "2") == 0)
            redeem_loyalty_flow(base, api_key, xdata_key, sec, id_token);
        else if (strcmp(ch, "3") == 0)
            redeem_allotment_flow(base, api_key, xdata_key, sec, id_token);
    }
}

/* ---------------------------------------------------------------------------
 * Transfer Pulsa (SHARE_BALANCE)
 *
 * Flow:
 *   1. Minta PIN 6 digit dari user.
 *   2. Minta receiver MSISDN (auto-normalize 08/62/+62).
 *   3. Minta amount (Rp).
 *   4. CIAM authorization-token/generate -> stage_token.
 *   5. balance_allotment(stage_token, receiver, amount).
 *
 * Source: porting dari purplemashu/me-cli app/menus/sharing.py.
 * Signature saat ini eksperimen (lihat crypto_helper.h).
 * ------------------------------------------------------------------------- */

static void transfer_pulsa_menu(const char* base_api,
                                const char* api_key,
                                const char* xdata_key,
                                const char* x_api_secret,
                                const char* id_token,
                                const char* access_token) {
    const char* base_ciam = getenv("BASE_CIAM_URL");
    if (!base_ciam || !*base_ciam) {
        printf("[-] BASE_CIAM_URL tidak di-set di .env\n");
        pause_enter();
        return;
    }
    const char* ua = getenv("UA");

    printf("\n=======================================================\n");
    printf("         TRANSFER PULSA (SHARE BALANCE)\n");
    printf("=======================================================\n");
    printf(" Pastikan PIN transaksi MyXL sudah di-set di app MyXL.\n");
    printf("-------------------------------------------------------\n");

    /* Guard: minimal Rp 5.000 saldo pulsa aktif */
    printf("[*] Cek saldo pulsa aktif...\n");
    double balance_rp = 0;
    cJSON* bal_res = get_balance(base_api, api_key, xdata_key, x_api_secret, id_token);
    if (bal_res) {
        cJSON* data = cJSON_GetObjectItem(bal_res, "data");
        if (data) {
            cJSON* bal = cJSON_GetObjectItem(data, "balance");
            if (bal) {
                if (cJSON_IsObject(bal)) {
                    cJSON* rem = cJSON_GetObjectItem(bal, "remaining");
                    if (rem && cJSON_IsNumber(rem)) balance_rp = rem->valuedouble;
                } else if (cJSON_IsNumber(bal)) {
                    balance_rp = bal->valuedouble;
                }
            }
        }
        cJSON_Delete(bal_res);
    }
    printf(" Saldo pulsa saat ini: Rp %.0f\n", balance_rp);
    if (balance_rp < 5000) {
        printf("\n[!] Saldo tidak cukup. Minimal Rp 5.000 untuk Transfer Pulsa.\n");
        pause_enter();
        return;
    }
    printf("-------------------------------------------------------\n");

    char pin[16]; read_line("PIN 6 digit (99=batal): ", pin, sizeof(pin));
    if (strcmp(pin, "99") == 0 || strlen(pin) != 6) { printf("[!] Dibatalkan.\n"); pause_enter(); return; }
    for (int i = 0; i < 6; i++) {
        if (pin[i] < '0' || pin[i] > '9') { printf("[-] PIN harus 6 digit angka.\n"); pause_enter(); return; }
    }

    char raw_msisdn[32]; read_line("Receiver MSISDN (08xxx / 628xxx): ", raw_msisdn, sizeof(raw_msisdn));
    if (strlen(raw_msisdn) == 0) { pause_enter(); return; }
    char* receiver_norm = normalize_msisdn(raw_msisdn);
    if (!receiver_norm) {
        printf("[-] Nomor MSISDN tidak valid.\n"); pause_enter(); return;
    }
    char receiver[32];
    snprintf(receiver, sizeof(receiver), "%s", receiver_norm);
    free(receiver_norm);

    char amt_str[16]; read_line("Amount transfer (Rp, min 5000): ", amt_str, sizeof(amt_str));
    int amount = atoi(amt_str);
    if (amount < 5000) { printf("[-] Amount minimal 5000.\n"); pause_enter(); return; }

    printf("\n  Tujuan : %s\n  Amount : Rp %d\n", receiver, amount);
    if (!read_yn("Lanjutkan? (y/n): ")) { printf("[!] Dibatalkan.\n"); pause_enter(); return; }

    printf("[*] Minta authorization_code (SHARE_BALANCE) dari CIAM...\n");
    char* stage_token = ciam_generate_share_balance_token(base_ciam, ua,
                                                          access_token, pin, receiver);
    if (!stage_token) {
        printf("[-] Gagal dapat stage_token. Pastikan PIN benar & nomor tujuan valid.\n");
        pause_enter(); return;
    }

    printf("[*] Kirim balance allotment...\n");
    cJSON* res = balance_allotment(base_api, api_key, xdata_key, x_api_secret,
                                   id_token, access_token, stage_token,
                                   receiver, amount);
    free(stage_token);

    if (!res) {
        printf("[-] Response balance_allotment NULL. Cek koneksi / signature.\n");
        pause_enter(); return;
    }

    const char* status = json_get_str(res, "status", "");
    if (strcmp(status, "SUCCESS") == 0) {
        printf("[+] Transfer pulsa sukses.\n");
    } else {
        char* dump = cJSON_Print(res);
        printf("[-] Transfer gagal. Response:\n%s\n", dump ? dump : "(null)");
        free(dump);
    }
    cJSON_Delete(res);
    pause_enter();
}

/* ---------------------------------------------------------------------------
 * Simpan Family Code
 *
 * File storage: /etc/engsel/family_bookmark.json
 * Format: array of objects
 *   [ { "family_code": "...", "name": "Kuota 3GB", "is_enterprise": false,
 *       "migration_type": "NONE" }, ... ]
 *
 * Saat user "Tambah", kita fetch nama family via get_family() dengan kombinasi
 * (is_enterprise=0/1, migration=NONE/PRE_TO_PRIOH/PRIOH_TO_PRIO/PRIO_TO_PRIOH) sampai
 * salah satu mengembalikan `data.package_family.name`. Kombinasi yang berhasil
 * ikut disimpan supaya saat membeli tidak perlu trial ulang.
 * ------------------------------------------------------------------------- */

static const char* SAVED_FAMILY_PATH = "/etc/engsel/family_bookmark.json";

static cJSON* sf_load_list(void) {
    size_t sz = 0;
    char* raw = file_read_all(SAVED_FAMILY_PATH, &sz);
    if (!raw || sz == 0) {
        free(raw);
        return cJSON_CreateArray();
    }
    cJSON* arr = cJSON_Parse(raw);
    free(raw);
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(arr);
        return cJSON_CreateArray();
    }
    return arr;
}

static int sf_save_list(cJSON* arr) {
    char* out = cJSON_Print(arr);
    if (!out) return -1;
    int rc = file_write_atomic(SAVED_FAMILY_PATH, out);
    free(out);
    return rc;
}

/* Coba get_family untuk semua kombinasi is_enterprise/migration.
 * Return 0 kalau ketemu; set out_name/out_ent/out_migration.
 * Return -1 kalau semua gagal. */
static int sf_resolve_family(const char* base_api, const char* api_key,
                             const char* xdata_key, const char* x_api_secret,
                             const char* id_token, const char* family_code,
                             char* out_name, size_t name_sz,
                             int* out_ent, char* out_mig, size_t mig_sz) {
    /* Migration types sinkron dengan do_family_bruteforce (main.c:232) */
    const char* migrations[] = { "NONE", "PRE_TO_PRIOH", "PRIOH_TO_PRIO", "PRIO_TO_PRIOH" };
    int ents[] = { 0, 1 };
    for (size_t i = 0; i < sizeof(migrations)/sizeof(migrations[0]); i++) {
        for (size_t j = 0; j < sizeof(ents)/sizeof(ents[0]); j++) {
            cJSON* res = get_family(base_api, api_key, xdata_key, x_api_secret,
                                    id_token, family_code, ents[j], migrations[i]);
            if (!res) continue;
            cJSON* data = cJSON_GetObjectItem(res, "data");
            if (data) {
                cJSON* pfam = cJSON_GetObjectItem(data, "package_family");
                if (pfam) {
                    const char* nm = json_get_str(pfam, "name", "");
                    if (nm && nm[0]) {
                        snprintf(out_name, name_sz, "%s", nm);
                        *out_ent = ents[j];
                        snprintf(out_mig, mig_sz, "%s", migrations[i]);
                        cJSON_Delete(res);
                        return 0;
                    }
                }
            }
            cJSON_Delete(res);
        }
    }
    return -1;
}

static void sf_add_entry(const char* base_api, const char* api_key,
                         const char* xdata_key, const char* x_api_secret,
                         const char* id_token) {
    char code[256]; read_line("Family Code (99=batal): ", code, sizeof(code));
    if (strcmp(code, "99") == 0 || strlen(code) == 0) return;

    printf("[*] Mencari nama family di XL Store...\n");
    char name[256] = "", mig[64] = "NONE";
    int ent = 0;
    if (sf_resolve_family(base_api, api_key, xdata_key, x_api_secret, id_token,
                          code, name, sizeof(name), &ent, mig, sizeof(mig)) != 0) {
        printf("[-] Family code tidak ditemukan di XL Store.\n");
        pause_enter();
        return;
    }

    cJSON* arr = sf_load_list();
    /* Cegah duplikat */
    cJSON* it; int exists = 0;
    cJSON_ArrayForEach(it, arr) {
        const char* fc = json_get_str(it, "family_code", "");
        if (strcmp(fc, code) == 0) { exists = 1; break; }
    }
    if (exists) {
        printf("[-] Family code ini sudah tersimpan.\n");
        cJSON_Delete(arr); pause_enter(); return;
    }

    cJSON* entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "family_code", code);
    cJSON_AddStringToObject(entry, "name", name);
    cJSON_AddBoolToObject(entry, "is_enterprise", ent);
    cJSON_AddStringToObject(entry, "migration_type", mig);
    cJSON_AddItemToArray(arr, entry);

    if (sf_save_list(arr) == 0) {
        printf("[+] Disimpan: %s (%s)\n", name, code);
    } else {
        printf("[-] Gagal menyimpan ke %s\n", SAVED_FAMILY_PATH);
    }
    cJSON_Delete(arr);
    pause_enter();
}

static void sf_delete_entry(int idx_1based) {
    cJSON* arr = sf_load_list();
    int n = cJSON_GetArraySize(arr);
    if (idx_1based < 1 || idx_1based > n) {
        printf("[-] Nomor di luar range.\n"); cJSON_Delete(arr); return;
    }
    cJSON* item = cJSON_GetArrayItem(arr, idx_1based - 1);
    char* nm = strdup(json_get_str(item, "name", "?"));
    cJSON_DeleteItemFromArray(arr, idx_1based - 1);
    if (sf_save_list(arr) == 0) printf("[+] Dihapus: %s\n", nm ? nm : "?");
    else printf("[-] Gagal simpan ke %s\n", SAVED_FAMILY_PATH);
    free(nm);
    cJSON_Delete(arr);
}

static void saved_family_menu(const char* base_api, const char* api_key,
                              const char* xdata_key, const char* x_api_secret,
                              const char* id_token) {
    while (1) {
        fx_clear();
        printf("=======================================================\n");
        printf("         SIMPAN FAMILY CODE\n");
        printf("=======================================================\n");

        cJSON* arr = sf_load_list();
        int n = cJSON_GetArraySize(arr);
        if (n == 0) {
            printf(" (belum ada family code tersimpan)\n");
        } else {
            for (int i = 0; i < n; i++) {
                cJSON* it = cJSON_GetArrayItem(arr, i);
                printf("%2d. %s\n", i + 1, json_get_str(it, "name", "?"));
                printf("    code: %s\n", json_get_str(it, "family_code", "?"));
            }
        }
        printf("-------------------------------------------------------\n");
        printf("Perintah:\n");
        printf("  <nomor>      Beli paket family tersebut\n");
        printf("  add          Tambah family code baru\n");
        printf("  del <nomor>  Hapus entry\n");
        printf("  00           Kembali\n");
        printf("Pilihan: ");
        fflush(stdout);

        char cmd[64]; if (!fgets(cmd, sizeof(cmd), stdin)) { cJSON_Delete(arr); return; }
        cmd[strcspn(cmd, "\n")] = 0;

        int del_n;
        if (strcmp(cmd, "00") == 0 || strcmp(cmd, "99") == 0) { cJSON_Delete(arr); return; }
        else if (cmd_eq(cmd, "add", "a")) {
            cJSON_Delete(arr);
            sf_add_entry(base_api, api_key, xdata_key, x_api_secret, id_token);
        }
        else if ((del_n = cmd_num(cmd, "del", "d")) >= 0) {
            cJSON_Delete(arr);
            sf_delete_entry(del_n);
            pause_enter();
        }
        else {
            int idx = atoi(cmd);
            if (idx >= 1 && idx <= n) {
                cJSON* item = cJSON_GetArrayItem(arr, idx - 1);
                char fc[256];
                snprintf(fc, sizeof(fc), "%s", json_get_str(item, "family_code", ""));
                cJSON_Delete(arr);
                if (fc[0]) purchase_flow_by_family_code(fc);
            } else {
                cJSON_Delete(arr);
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Custom Decoy
 *
 * File storage: /etc/engsel/custom_decoy.json
 *   {
 *     "active": 2,
 *     "entries": [
 *       { "subs_type": "PRIORITAS",
 *         "family_code": "...", "variant_code": "...", "order": 1,
 *         "name": "Akrab 50GB", "is_enterprise": false,
 *         "migration_type": "NONE" }, ... ] }
 *
 * Menu hanya menampilkan entry yang subs_type-nya cocok dengan tipe kartu
 * aktif (`active_subs_type`). Saat menambah, subs_type entry diset otomatis
 * ke tipe kartu saat itu.
 * ------------------------------------------------------------------------- */

static const char* CUSTOM_DECOY_PATH = "/etc/engsel/custom_decoy.json";

static cJSON* cd_load(void) {
    size_t sz = 0;
    char* raw = file_read_all(CUSTOM_DECOY_PATH, &sz);
    if (!raw || sz == 0) {
        free(raw);
        cJSON* o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "active", 0);
        cJSON_AddItemToObject(o, "entries", cJSON_CreateArray());
        return o;
    }
    cJSON* o = cJSON_Parse(raw); free(raw);
    if (!o) {
        cJSON_Delete(o);
        o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "active", 0);
        cJSON_AddItemToObject(o, "entries", cJSON_CreateArray());
        return o;
    }
    if (!cJSON_GetObjectItem(o, "active"))  cJSON_AddNumberToObject(o, "active", 0);
    if (!cJSON_GetObjectItem(o, "entries")) cJSON_AddItemToObject(o, "entries", cJSON_CreateArray());
    return o;
}

static int cd_save(cJSON* o) {
    char* out = cJSON_Print(o);
    if (!out) return -1;
    int rc = file_write_atomic(CUSTOM_DECOY_PATH, out);
    free(out);
    return rc;
}

/* Fetch full family JSON untuk browse varian+opsi (coba kombinasi enterprise/migration). */
static cJSON* cd_fetch_family(const char* base_api, const char* api_key,
                              const char* xdata_key, const char* x_api_secret,
                              const char* id_token, const char* family_code,
                              int* out_ent, char* out_mig, size_t mig_sz) {
    /* Migration types sinkron dengan do_family_bruteforce (main.c:232) */
    const char* migrations[] = { "NONE", "PRE_TO_PRIOH", "PRIOH_TO_PRIO", "PRIO_TO_PRIOH" };
    int ents[] = { 0, 1 };
    for (size_t i = 0; i < sizeof(migrations)/sizeof(migrations[0]); i++) {
        for (size_t j = 0; j < sizeof(ents)/sizeof(ents[0]); j++) {
            cJSON* r = get_family(base_api, api_key, xdata_key, x_api_secret,
                                  id_token, family_code, ents[j], migrations[i]);
            if (!r) continue;
            cJSON* data = cJSON_GetObjectItem(r, "data");
            if (data) {
                cJSON* variants = cJSON_GetObjectItem(data, "package_variants");
                if (variants && cJSON_GetArraySize(variants) > 0) {
                    *out_ent = ents[j];
                    snprintf(out_mig, mig_sz, "%s", migrations[i]);
                    return r;
                }
            }
            cJSON_Delete(r);
        }
    }
    return NULL;
}

static void cd_add(const char* base_api, const char* api_key,
                   const char* xdata_key, const char* x_api_secret,
                   const char* id_token) {
    printf("\n[*] Tipe kartu aktif untuk entry ini: %s\n", active_subs_type);

    char code[256]; read_line("Family Code untuk decoy (99=batal): ", code, sizeof(code));
    if (strcmp(code, "99") == 0 || strlen(code) == 0) return;

    printf("[*] Fetching family dari XL Store...\n");
    int ent = 0; char mig[64] = "NONE";
    cJSON* fam = cd_fetch_family(base_api, api_key, xdata_key, x_api_secret,
                                 id_token, code, &ent, mig, sizeof(mig));
    if (!fam) { printf("[-] Family code tidak ditemukan.\n"); pause_enter(); return; }

    /* Susun flat list (variant_code, order, option_code, name, price) */
    typedef struct { const char* vc; int ord; const char* oc; const char* name; int price; } item_t;
    item_t items[256]; int nitems = 0;
    cJSON* data = cJSON_GetObjectItem(fam, "data");
    cJSON* pfam = cJSON_GetObjectItem(data, "package_family");
    const char* fam_name = json_get_str(pfam, "name", "?");
    cJSON* variants = cJSON_GetObjectItem(data, "package_variants");
    cJSON* v;
    cJSON_ArrayForEach(v, variants) {
        const char* vc = json_get_str(v, "package_variant_code", "");
        cJSON* opts = cJSON_GetObjectItem(v, "package_options");
        cJSON* o;
        cJSON_ArrayForEach(o, opts) {
            if (nitems >= 256) break;
            items[nitems].vc    = vc;
            items[nitems].ord   = cJSON_GetObjectItem(o, "order") ? cJSON_GetObjectItem(o, "order")->valueint : 0;
            items[nitems].oc    = json_get_str(o, "package_option_code", "");
            items[nitems].name  = json_get_str(o, "name", "?");
            items[nitems].price = cJSON_GetObjectItem(o, "price") ? cJSON_GetObjectItem(o, "price")->valueint : 0;
            nitems++;
        }
    }

    if (nitems == 0) {
        printf("[-] Family ini tidak punya opsi.\n"); cJSON_Delete(fam); pause_enter(); return;
    }

    printf("\n-------------------------------------------------------\n");
    printf(" %s\n", fam_name);
    printf("-------------------------------------------------------\n");
    for (int i = 0; i < nitems; i++) {
        printf("%2d. %s  (Rp %d)\n", i + 1, items[i].name, items[i].price);
    }
    printf("-------------------------------------------------------\n");
    char pick[16]; read_line("Pilih nomor opsi (99=batal): ", pick, sizeof(pick));
    if (strcmp(pick, "99") == 0) { cJSON_Delete(fam); return; }
    int idx = atoi(pick);
    if (idx < 1 || idx > nitems) {
        printf("[-] Nomor di luar range.\n"); cJSON_Delete(fam); pause_enter(); return;
    }
    item_t* sel = &items[idx - 1];

    cJSON* cfg = cd_load();
    cJSON* ents_arr = cJSON_GetObjectItem(cfg, "entries");
    cJSON* entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "subs_type",      active_subs_type);
    cJSON_AddStringToObject(entry, "family_code",    code);
    cJSON_AddStringToObject(entry, "variant_code",   sel->vc);
    cJSON_AddNumberToObject(entry, "order",          sel->ord);
    cJSON_AddStringToObject(entry, "name",           sel->name);
    cJSON_AddBoolToObject  (entry, "is_enterprise",  ent);
    cJSON_AddStringToObject(entry, "migration_type", mig);
    cJSON_AddItemToArray(ents_arr, entry);

    if (cd_save(cfg) == 0) printf("[+] Decoy custom disimpan: %s\n", sel->name);
    else                   printf("[-] Gagal simpan ke %s\n", CUSTOM_DECOY_PATH);
    cJSON_Delete(cfg);
    cJSON_Delete(fam);
    pause_enter();
}

static void custom_decoy_menu(const char* base_api, const char* api_key,
                              const char* xdata_key, const char* x_api_secret,
                              const char* id_token) {
    while (1) {
        fx_clear();
        printf("=======================================================\n");
        printf("         CUSTOM DECOY  (tipe kartu: %s)\n", active_subs_type);
        printf("=======================================================\n");

        cJSON* cfg = cd_load();
        cJSON* ents_arr = cJSON_GetObjectItem(cfg, "entries");
        cJSON* act = cJSON_GetObjectItem(cfg, "active");
        int aidx = (act && cJSON_IsNumber(act)) ? act->valueint : 0;
        int ntotal = cJSON_GetArraySize(ents_arr);

        /* Display map: vis_idx (1..M) -> real_idx (1..N). M = jumlah entry
         * yang subs_type-nya match. */
        int real_map[512]; int nvis = 0;
        for (int i = 0; i < ntotal; i++) {
            cJSON* e = cJSON_GetArrayItem(ents_arr, i);
            const char* st = json_get_str(e, "subs_type", "");
            if (strcmp(st, active_subs_type) == 0) {
                if (nvis >= (int)(sizeof(real_map)/sizeof(real_map[0]))) break;
                real_map[nvis++] = i + 1;
            }
        }

        if (nvis == 0) {
            printf(" (belum ada custom decoy untuk tipe %s)\n", active_subs_type);
        } else {
            for (int i = 0; i < nvis; i++) {
                cJSON* e = cJSON_GetArrayItem(ents_arr, real_map[i] - 1);
                int is_active = (real_map[i] == aidx);
                printf("%2d. %s%s\n", i + 1, json_get_str(e, "name", "?"),
                       is_active ? "   [ACTIVE]" : "");
                printf("    family: %s  order: %d\n",
                       json_get_str(e, "family_code", "?"),
                       cJSON_GetObjectItem(e, "order") ? cJSON_GetObjectItem(e, "order")->valueint : 0);
            }
        }
        if (aidx == 0) printf("\n (decoy aktif: default bawaan)\n");

        printf("-------------------------------------------------------\n");
        printf("Perintah:\n");
        printf("  add           Tambah custom decoy baru\n");
        printf("  use <nomor>   Jadikan entry aktif (decoy ini dipakai saat beli)\n");
        printf("  use 0         Kembali ke decoy default bawaan\n");
        printf("  del <nomor>   Hapus entry\n");
        printf("  00            Kembali\n");
        printf("Pilihan: ");
        fflush(stdout);

        char cmd[64]; if (!fgets(cmd, sizeof(cmd), stdin)) { cJSON_Delete(cfg); return; }
        cmd[strcspn(cmd, "\n")] = 0;

        int del_n;
        if (strcmp(cmd, "00") == 0 || strcmp(cmd, "99") == 0) { cJSON_Delete(cfg); return; }
        else if (cmd_eq(cmd, "add", "a")) {
            cJSON_Delete(cfg);
            cd_add(base_api, api_key, xdata_key, x_api_secret, id_token);
        }
        else if (strncasecmp(cmd, "use ", 4) == 0 || strncasecmp(cmd, "use\t", 4) == 0) {
            const char* p = cmd + 3;
            while (*p == ' ' || *p == '\t') p++;
            int v = atoi(p);
            int new_active = 0;
            if (strcmp(p, "0") == 0) new_active = 0;
            else if (v >= 1 && v <= nvis) new_active = real_map[v - 1];
            else { printf("[-] Nomor di luar range.\n"); cJSON_Delete(cfg); pause_enter(); continue; }
            cJSON_ReplaceItemInObject(cfg, "active", cJSON_CreateNumber(new_active));
            if (cd_save(cfg) == 0) {
                if (new_active == 0) printf("[+] Decoy aktif: default bawaan\n");
                else {
                    cJSON* e = cJSON_GetArrayItem(ents_arr, new_active - 1);
                    printf("[+] Decoy aktif: %s\n", json_get_str(e, "name", "?"));
                }
            } else printf("[-] Gagal simpan.\n");
            cJSON_Delete(cfg); pause_enter();
        }
        else if ((del_n = cmd_num(cmd, "del", "d")) >= 0) {
            if (del_n < 1 || del_n > nvis) { printf("[-] Nomor di luar range.\n"); cJSON_Delete(cfg); pause_enter(); continue; }
            int real_idx = real_map[del_n - 1];
            cJSON* e = cJSON_GetArrayItem(ents_arr, real_idx - 1);
            const char* nm = json_get_str(e, "name", "?");
            printf("Hapus %s? (y/n): ", nm);
            if (read_yn("")) {
                cJSON_DeleteItemFromArray(ents_arr, real_idx - 1);
                /* Adjust active index */
                if (aidx == real_idx) cJSON_ReplaceItemInObject(cfg, "active", cJSON_CreateNumber(0));
                else if (aidx > real_idx) cJSON_ReplaceItemInObject(cfg, "active", cJSON_CreateNumber(aidx - 1));
                if (cd_save(cfg) == 0) printf("[+] Dihapus.\n");
                else printf("[-] Gagal simpan.\n");
            }
            cJSON_Delete(cfg); pause_enter();
        }
        else {
            cJSON_Delete(cfg);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Custom Paket HOT
 *
 * File storage: /etc/engsel/hot_data/hot.json (dipakai juga oleh menu 3 HOT)
 * Schema per entry:
 *   { "family_name", "family_code", "is_enterprise", "variant_name",
 *     "option_name", "order" }
 * ------------------------------------------------------------------------- */

static const char* HOT_JSON_PATH      = "/etc/engsel/hot_data/hot.json";
/* OpenWrt memisahkan rootfs (overlay) dan firmware asli (/rom). File bawaan
 * paket tersedia di /rom walau /etc-nya sudah dimodifikasi. */
static const char* HOT_JSON_ROM_PATH  = "/rom/etc/engsel/hot_data/hot.json";

static cJSON* ch_load(void) {
    size_t sz = 0;
    char* raw = file_read_all(HOT_JSON_PATH, &sz);
    if (!raw || sz == 0) { free(raw); return cJSON_CreateArray(); }
    cJSON* arr = cJSON_Parse(raw); free(raw);
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(arr); return cJSON_CreateArray();
    }
    return arr;
}

static int ch_save(cJSON* arr) {
    char* out = cJSON_Print(arr);
    if (!out) return -1;
    int rc = file_write_atomic(HOT_JSON_PATH, out);
    free(out);
    return rc;
}

/* Kembalikan hot.json ke bawaan paket (file di /rom saat di OpenWrt).
 * Return 0 sukses, -1 kalau sumber bawaan tidak ada / tulis gagal. */
static int ch_reset_to_default(void) {
    size_t sz = 0;
    char* raw = file_read_all(HOT_JSON_ROM_PATH, &sz);
    if (!raw || sz == 0) { free(raw); return -1; }
    /* Validasi JSON supaya tidak overwrite dengan file rusak. */
    cJSON* test = cJSON_Parse(raw);
    if (!test) { free(raw); return -1; }
    cJSON_Delete(test);
    int rc = file_write_atomic(HOT_JSON_PATH, raw);
    free(raw);
    return rc;
}

static void ch_add(const char* base_api, const char* api_key,
                   const char* xdata_key, const char* x_api_secret,
                   const char* id_token) {
    char code[256]; read_line("Family Code (99=batal): ", code, sizeof(code));
    if (strcmp(code, "99") == 0 || strlen(code) == 0) return;

    printf("[*] Fetching family dari XL Store...\n");
    int ent = 0; char mig[64] = "NONE";
    cJSON* fam = cd_fetch_family(base_api, api_key, xdata_key, x_api_secret,
                                 id_token, code, &ent, mig, sizeof(mig));
    if (!fam) { printf("[-] Family code tidak ditemukan.\n"); pause_enter(); return; }

    cJSON* data = cJSON_GetObjectItem(fam, "data");
    cJSON* pfam = cJSON_GetObjectItem(data, "package_family");
    const char* fam_name = json_get_str(pfam, "name", "?");

    typedef struct { const char* vn; int ord; const char* on; int price; } it_t;
    it_t items[256]; int nitems = 0;
    cJSON* variants = cJSON_GetObjectItem(data, "package_variants");
    cJSON* v;
    cJSON_ArrayForEach(v, variants) {
        const char* vn = json_get_str(v, "name", "");
        cJSON* opts = cJSON_GetObjectItem(v, "package_options");
        cJSON* o;
        cJSON_ArrayForEach(o, opts) {
            if (nitems >= 256) break;
            items[nitems].vn    = vn;
            items[nitems].ord   = cJSON_GetObjectItem(o, "order") ? cJSON_GetObjectItem(o, "order")->valueint : 0;
            items[nitems].on    = json_get_str(o, "name", "?");
            items[nitems].price = cJSON_GetObjectItem(o, "price") ? cJSON_GetObjectItem(o, "price")->valueint : 0;
            nitems++;
        }
    }
    if (nitems == 0) { printf("[-] Family tidak punya opsi.\n"); cJSON_Delete(fam); pause_enter(); return; }

    printf("\n-------------------------------------------------------\n");
    printf(" %s\n", fam_name);
    printf("-------------------------------------------------------\n");
    for (int i = 0; i < nitems; i++) {
        printf("%2d. %s - %s  (Rp %d)\n", i + 1, items[i].vn, items[i].on, items[i].price);
    }
    printf("-------------------------------------------------------\n");
    char pick[16]; read_line("Pilih nomor opsi (99=batal): ", pick, sizeof(pick));
    if (strcmp(pick, "99") == 0) { cJSON_Delete(fam); return; }
    int idx = atoi(pick);
    if (idx < 1 || idx > nitems) { printf("[-] Nomor di luar range.\n"); cJSON_Delete(fam); pause_enter(); return; }
    it_t* sel = &items[idx - 1];

    cJSON* arr = ch_load();
    cJSON* entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "family_name",   fam_name);
    cJSON_AddStringToObject(entry, "family_code",   code);
    cJSON_AddBoolToObject  (entry, "is_enterprise", ent);
    cJSON_AddStringToObject(entry, "variant_name",  sel->vn);
    cJSON_AddStringToObject(entry, "option_name",   sel->on);
    cJSON_AddNumberToObject(entry, "order",         sel->ord);
    cJSON_AddItemToArray(arr, entry);

    if (ch_save(arr) == 0) printf("[+] Ditambahkan ke Paket HOT: %s - %s\n", sel->vn, sel->on);
    else                   printf("[-] Gagal simpan ke %s\n", HOT_JSON_PATH);
    cJSON_Delete(arr);
    cJSON_Delete(fam);
    pause_enter();
}

static void custom_hot_menu(const char* base_api, const char* api_key,
                            const char* xdata_key, const char* x_api_secret,
                            const char* id_token) {
    while (1) {
        fx_clear();
        printf("=======================================================\n");
        printf("         CUSTOM PAKET HOT\n");
        printf("=======================================================\n");

        cJSON* arr = ch_load();
        int n = cJSON_GetArraySize(arr);
        if (n == 0) {
            printf(" (belum ada paket HOT)\n");
        } else {
            for (int i = 0; i < n; i++) {
                cJSON* e = cJSON_GetArrayItem(arr, i);
                printf("%2d. %s - %s - %s\n", i + 1,
                       json_get_str(e, "family_name", "?"),
                       json_get_str(e, "variant_name", "?"),
                       json_get_str(e, "option_name", "?"));
            }
        }
        printf("-------------------------------------------------------\n");
        printf("Perintah:\n");
        printf("  add          Tambah paket HOT baru\n");
        printf("  del <nomor>  Hapus entry\n");
        printf("  reset        Kembalikan ke paket HOT bawaan\n");
        printf("  00           Kembali\n");
        printf("Pilihan: ");
        fflush(stdout);

        char cmd[64]; if (!fgets(cmd, sizeof(cmd), stdin)) { cJSON_Delete(arr); return; }
        cmd[strcspn(cmd, "\n")] = 0;

        int del_n;
        if (strcmp(cmd, "00") == 0 || strcmp(cmd, "99") == 0) { cJSON_Delete(arr); return; }
        else if (cmd_eq(cmd, "add", "a")) {
            cJSON_Delete(arr);
            ch_add(base_api, api_key, xdata_key, x_api_secret, id_token);
        }
        else if ((del_n = cmd_num(cmd, "del", "d")) >= 0) {
            if (del_n < 1 || del_n > n) { printf("[-] Nomor di luar range.\n"); cJSON_Delete(arr); pause_enter(); continue; }
            cJSON* e = cJSON_GetArrayItem(arr, del_n - 1);
            const char* nm = json_get_str(e, "option_name", "?");
            printf("Hapus %s? (y/n): ", nm);
            if (read_yn("")) {
                cJSON_DeleteItemFromArray(arr, del_n - 1);
                if (ch_save(arr) == 0) printf("[+] Dihapus.\n");
                else                   printf("[-] Gagal simpan.\n");
            }
            cJSON_Delete(arr); pause_enter();
        }
        else if (cmd_eq(cmd, "reset", "r")) {
            cJSON_Delete(arr);
            printf("Reset paket HOT ke bawaan? Semua entry tambahan akan hilang. (y/n): ");
            if (read_yn("")) {
                if (ch_reset_to_default() == 0) {
                    printf("[+] hot.json dikembalikan dari %s\n", HOT_JSON_ROM_PATH);
                } else {
                    printf("[-] Gagal reset. Sumber bawaan (%s) tidak ditemukan\n"
                           "    atau file tidak bisa ditulis. Di sistem non-OpenWrt\n"
                           "    /rom biasanya tidak ada — reinstall paket untuk kembali\n"
                           "    ke bawaan.\n", HOT_JSON_ROM_PATH);
                }
            }
            pause_enter();
        }
        else {
            cJSON_Delete(arr);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Main dispatcher
 * ------------------------------------------------------------------------- */

void show_features_menu(const char* base_api, const char* api_key,
                        const char* xdata_key, const char* x_api_secret,
                        const char* enc_field_key,
                        const char* id_token, const char* access_token,
                        const char* my_msisdn) {
    if (!id_token || !id_token[0]) { printf("[-] Belum login.\n"); return; }
    while (1) {
        fx_clear();
        printf("=======================================================\n");
        printf("         FITUR LANJUTAN\n");
        printf("=======================================================\n");
        printf("1. Circle\n"
               "2. Family Plan/Akrab Organizer\n"
               "3. Transfer Pulsa\n"
               "4. Simpan Family Code\n"
               "5. Custom Decoy\n"
               "6. Custom Paket HOT\n"
               "7. Auto Buy\n"
               "-------------------------------------------------------\n"
               "00. Kembali\n"
               "99. Menu utama\n"
               "Pilihan: ");
        fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) return;
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0 || strcmp(ch, "99") == 0) return;
        else if (strcmp(ch, "1") == 0)
            circle_info_menu(base_api, api_key, xdata_key, x_api_secret,
                             enc_field_key, id_token, access_token, my_msisdn);
        else if (strcmp(ch, "2") == 0)
            famplan_menu(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "3") == 0)
            transfer_pulsa_menu(base_api, api_key, xdata_key, x_api_secret,
                                id_token, access_token);
        else if (strcmp(ch, "4") == 0)
            saved_family_menu(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "5") == 0)
            custom_decoy_menu(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "6") == 0)
            custom_hot_menu(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "7") == 0)
            auto_buy_menu(base_api, api_key, xdata_key, x_api_secret,
                          enc_field_key, id_token);
    }
}

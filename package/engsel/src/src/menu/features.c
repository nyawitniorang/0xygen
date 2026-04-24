#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "../include/cJSON.h"
#include "../include/menu/features.h"
#include "../include/client/circle.h"
#include "../include/client/famplan.h"
#include "../include/client/store.h"
#include "../include/client/redeem.h"
#include "../include/service/crypto_aes.h"
#include "../include/util/json_util.h"
#include "../include/util/phone.h"

/* ---------------------------------------------------------------------------
 * Helper utilities
 * ------------------------------------------------------------------------- */

#define W 55

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
 * Main dispatcher
 * ------------------------------------------------------------------------- */

void show_features_menu(const char* base_api, const char* api_key,
                        const char* xdata_key, const char* x_api_secret,
                        const char* enc_field_key,
                        const char* id_token, const char* access_token,
                        const char* my_msisdn) {
    if (!id_token || !id_token[0]) { printf("[-] Belum login.\n"); return; }
    while (1) {
        printf("\n=== FITUR LANJUTAN ===\n"
               "1. Circle (status, invite, remove, bonus)\n"
               "2. Family Plan (members, change, limit, remove)\n"
               "3. Store (family-list, search, segments, notifications)\n"
               "4. Redeem (bounty/loyalty/allotment)\n"
               "00. Kembali\n"
               "Pilihan: ");
        fflush(stdout);
        char ch[16]; if (!fgets(ch, sizeof(ch), stdin)) return;
        ch[strcspn(ch, "\n")] = 0;
        if (strcmp(ch, "00") == 0) return;
        else if (strcmp(ch, "1") == 0)
            circle_info_menu(base_api, api_key, xdata_key, x_api_secret,
                             enc_field_key, id_token, access_token, my_msisdn);
        else if (strcmp(ch, "2") == 0)
            famplan_menu(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "3") == 0)
            store_menu(base_api, api_key, xdata_key, x_api_secret, id_token);
        else if (strcmp(ch, "4") == 0)
            redeem_menu(base_api, api_key, xdata_key, x_api_secret,
                        enc_field_key, id_token, access_token);
    }
}

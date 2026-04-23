#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/cJSON.h"
#include "../include/menu/features.h"
#include "../include/client/circle.h"
#include "../include/client/famplan.h"
#include "../include/client/store.h"
#include "../include/client/redeem.h"
#include "../include/util/json_util.h"

static void flush_line(void) {
    int c; while ((c = getchar()) != '\n' && c != EOF);
}

static void read_line(const char* prompt, char* buf, size_t n) {
    printf("%s", prompt); fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
}

static void dump_result(const char* label, cJSON* res) {
    printf("\n=== %s ===\n", label);
    if (!res) { printf("[-] Tidak ada response.\n"); return; }
    char* out = cJSON_Print(res);
    if (out) { printf("%s\n", out); free(out); }
    cJSON_Delete(res);
    printf("\nTekan Enter untuk melanjutkan...");
    fflush(stdout); flush_line();
}

void show_features_menu(const char* base_api, const char* api_key,
                        const char* xdata_key, const char* x_api_secret,
                        const char* id_token, const char* access_token) {
    if (!id_token || !id_token[0]) {
        printf("[-] Belum login.\n");
        return;
    }
    char choice[16];
    while (1) {
        printf("\n=== FITUR LANJUTAN ===\n"
               "1. Circle: status group saya\n"
               "2. Circle: anggota group (perlu group_id)\n"
               "3. Family Plan: member-info saya\n"
               "4. Family Plan: validasi MSISDN (check-dukcapil)\n"
               "5. Store: family-list (PREPAID)\n"
               "6. Store: daftar paket (search v9, PREPAID)\n"
               "7. Store: segments\n"
               "8. Store: redeemables\n"
               "9. Store: notifications\n"
               "00. Kembali\n"
               "Pilihan: ");
        fflush(stdout);
        if (!fgets(choice, sizeof(choice), stdin)) return;
        choice[strcspn(choice, "\n")] = 0;

        if (strcmp(choice, "00") == 0) return;
        else if (strcmp(choice, "1") == 0) {
            dump_result("circle_get_group_data",
                circle_get_group_data(base_api, api_key, xdata_key, x_api_secret, id_token));
        }
        else if (strcmp(choice, "2") == 0) {
            char gid[128]; read_line("group_id: ", gid, sizeof(gid));
            dump_result("circle_get_group_members",
                circle_get_group_members(base_api, api_key, xdata_key, x_api_secret,
                                         id_token, gid));
        }
        else if (strcmp(choice, "3") == 0) {
            dump_result("famplan_get_member_info",
                famplan_get_member_info(base_api, api_key, xdata_key, x_api_secret, id_token));
        }
        else if (strcmp(choice, "4") == 0) {
            char msisdn[32]; read_line("MSISDN target: ", msisdn, sizeof(msisdn));
            dump_result("famplan_validate_msisdn",
                famplan_validate_msisdn(base_api, api_key, xdata_key, x_api_secret,
                                        id_token, msisdn));
        }
        else if (strcmp(choice, "5") == 0) {
            dump_result("store_get_family_list",
                store_get_family_list(base_api, api_key, xdata_key, x_api_secret,
                                      id_token, "PREPAID", 0));
        }
        else if (strcmp(choice, "6") == 0) {
            dump_result("store_get_packages",
                store_get_packages(base_api, api_key, xdata_key, x_api_secret,
                                   id_token, "PREPAID", 0));
        }
        else if (strcmp(choice, "7") == 0) {
            dump_result("store_get_segments",
                store_get_segments(base_api, api_key, xdata_key, x_api_secret,
                                   id_token, 0));
        }
        else if (strcmp(choice, "8") == 0) {
            dump_result("store_get_redeemables",
                store_get_redeemables(base_api, api_key, xdata_key, x_api_secret,
                                      id_token, 0));
        }
        else if (strcmp(choice, "9") == 0) {
            dump_result("store_get_notifications",
                store_get_notifications(base_api, api_key, xdata_key, x_api_secret,
                                        id_token));
        }
        else {
            printf("[-] Pilihan tidak valid.\n");
        }
        (void)access_token;
    }
}

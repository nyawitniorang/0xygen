#ifndef ENGSEL_CLIENT_CIRCLE_H
#define ENGSEL_CLIENT_CIRCLE_H

#include "../cJSON.h"

/* Semua fungsi mengembalikan cJSON* yang sudah di-decrypt (harus di-cJSON_Delete).
   base = BASE_API_URL, api_key = API_KEY, xdata_key = XDATA_KEY,
   sec = X_API_BASE_SECRET, enc_field_key = ENCRYPTED_FIELD_KEY (untuk encrypt MSISDN),
   id_token = tokens.id_token, access_token dipakai pada endpoint yang memerlukan. */

cJSON* circle_get_group_data(const char* base, const char* api_key, const char* xdata_key,
                             const char* sec, const char* id_token);

cJSON* circle_get_group_members(const char* base, const char* api_key, const char* xdata_key,
                                const char* sec, const char* id_token, const char* group_id);

cJSON* circle_validate_member(const char* base, const char* api_key, const char* xdata_key,
                              const char* sec, const char* enc_field_key,
                              const char* id_token, const char* msisdn);

cJSON* circle_invite_member(const char* base, const char* api_key, const char* xdata_key,
                            const char* sec, const char* enc_field_key,
                            const char* id_token, const char* access_token,
                            const char* msisdn, const char* name,
                            const char* group_id, const char* member_id_parent);

cJSON* circle_remove_member(const char* base, const char* api_key, const char* xdata_key,
                            const char* sec, const char* id_token,
                            const char* member_id, const char* group_id,
                            const char* member_id_parent, int is_last_member);

cJSON* circle_accept_invitation(const char* base, const char* api_key, const char* xdata_key,
                                const char* sec, const char* id_token, const char* access_token,
                                const char* group_id, const char* member_id);

cJSON* circle_create_group(const char* base, const char* api_key, const char* xdata_key,
                           const char* sec, const char* enc_field_key,
                           const char* id_token, const char* access_token,
                           const char* parent_name, const char* group_name,
                           const char* member_msisdn, const char* member_name);

cJSON* circle_spending_tracker(const char* base, const char* api_key, const char* xdata_key,
                               const char* sec, const char* id_token,
                               const char* parent_subs_id, const char* family_id);

cJSON* circle_get_bonus_list(const char* base, const char* api_key, const char* xdata_key,
                             const char* sec, const char* id_token,
                             const char* parent_subs_id, const char* family_id);

#endif

#include <stdlib.h>
#include <string.h>
#include "../include/client/circle.h"
#include "../include/client/engsel.h"
#include "../include/service/crypto_aes.h"

static cJSON* post(const char* base, const char* key, const char* xdata,
                   const char* sec, const char* path, cJSON* payload, const char* id_token) {
    return send_api_request(base, key, xdata, sec, path, payload, id_token, "POST", NULL);
}

cJSON* circle_get_group_data(const char* base, const char* api_key, const char* xdata_key,
                             const char* sec, const char* id_token) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec, "family-hub/api/v8/groups/status", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* circle_get_group_members(const char* base, const char* api_key, const char* xdata_key,
                                const char* sec, const char* id_token, const char* group_id) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "group_id", group_id ? group_id : "");
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec, "family-hub/api/v8/members/info", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* circle_validate_member(const char* base, const char* api_key, const char* xdata_key,
                              const char* sec, const char* enc_field_key,
                              const char* id_token, const char* msisdn) {
    char* enc_msisdn = encrypt_circle_msisdn(msisdn, enc_field_key);
    cJSON* p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "msisdn", enc_msisdn ? enc_msisdn : "");
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec, "family-hub/api/v8/members/validate", p, id_token);
    cJSON_Delete(p);
    free(enc_msisdn);
    return r;
}

cJSON* circle_invite_member(const char* base, const char* api_key, const char* xdata_key,
                            const char* sec, const char* enc_field_key,
                            const char* id_token, const char* access_token,
                            const char* msisdn, const char* name,
                            const char* group_id, const char* member_id_parent) {
    char* enc_msisdn = encrypt_circle_msisdn(msisdn, enc_field_key);
    cJSON* p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "access_token", access_token ? access_token : "");
    cJSON_AddStringToObject(p, "group_id", group_id ? group_id : "");
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON* members = cJSON_AddArrayToObject(p, "members");
    cJSON* m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "msisdn", enc_msisdn ? enc_msisdn : "");
    cJSON_AddStringToObject(m, "name", name ? name : "");
    cJSON_AddItemToArray(members, m);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON_AddStringToObject(p, "member_id_parent", member_id_parent ? member_id_parent : "");
    cJSON* r = post(base, api_key, xdata_key, sec, "family-hub/api/v8/members/invite", p, id_token);
    cJSON_Delete(p);
    free(enc_msisdn);
    return r;
}

cJSON* circle_remove_member(const char* base, const char* api_key, const char* xdata_key,
                            const char* sec, const char* id_token,
                            const char* member_id, const char* group_id,
                            const char* member_id_parent, int is_last_member) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "member_id", member_id ? member_id : "");
    cJSON_AddStringToObject(p, "group_id", group_id ? group_id : "");
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddBoolToObject(p, "is_last_member", is_last_member ? 1 : 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON_AddStringToObject(p, "member_id_parent", member_id_parent ? member_id_parent : "");
    cJSON* r = post(base, api_key, xdata_key, sec, "family-hub/api/v8/members/remove", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* circle_accept_invitation(const char* base, const char* api_key, const char* xdata_key,
                                const char* sec, const char* id_token, const char* access_token,
                                const char* group_id, const char* member_id) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "access_token", access_token ? access_token : "");
    cJSON_AddStringToObject(p, "group_id", group_id ? group_id : "");
    cJSON_AddStringToObject(p, "member_id", member_id ? member_id : "");
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "family-hub/api/v8/groups/accept-invitation", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* circle_create_group(const char* base, const char* api_key, const char* xdata_key,
                           const char* sec, const char* enc_field_key,
                           const char* id_token, const char* access_token,
                           const char* parent_name, const char* group_name,
                           const char* member_msisdn, const char* member_name) {
    char* enc_msisdn = encrypt_circle_msisdn(member_msisdn, enc_field_key);
    cJSON* p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "access_token", access_token ? access_token : "");
    cJSON_AddStringToObject(p, "parent_name", parent_name ? parent_name : "");
    cJSON_AddStringToObject(p, "group_name", group_name ? group_name : "");
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON* members = cJSON_AddArrayToObject(p, "members");
    cJSON* m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "msisdn", enc_msisdn ? enc_msisdn : "");
    cJSON_AddStringToObject(m, "name", member_name ? member_name : "");
    cJSON_AddItemToArray(members, m);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec, "family-hub/api/v8/groups/create", p, id_token);
    cJSON_Delete(p);
    free(enc_msisdn);
    return r;
}

cJSON* circle_spending_tracker(const char* base, const char* api_key, const char* xdata_key,
                               const char* sec, const char* id_token,
                               const char* parent_subs_id, const char* family_id) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "parent_subs_id", parent_subs_id ? parent_subs_id : "");
    cJSON_AddStringToObject(p, "family_id", family_id ? family_id : "");
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "gamification/api/v8/family-hub/spending-tracker", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* circle_get_bonus_list(const char* base, const char* api_key, const char* xdata_key,
                             const char* sec, const char* id_token,
                             const char* parent_subs_id, const char* family_id) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "parent_subs_id", parent_subs_id ? parent_subs_id : "");
    cJSON_AddStringToObject(p, "family_id", family_id ? family_id : "");
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "gamification/api/v8/family-hub/bonus/list", p, id_token);
    cJSON_Delete(p);
    return r;
}

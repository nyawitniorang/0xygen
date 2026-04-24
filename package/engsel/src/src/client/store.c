#include "../include/client/store.h"
#include "../include/client/engsel.h"

static cJSON* post(const char* base, const char* key, const char* xdata, const char* sec,
                   const char* path, cJSON* payload, const char* id_token) {
    return send_api_request(base, key, xdata, sec, path, payload, id_token, "POST", NULL);
}

cJSON* store_get_family_list(const char* base, const char* api_key, const char* xdata_key,
                             const char* sec, const char* id_token,
                             const char* subs_type, int is_enterprise) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", is_enterprise ? 1 : 0);
    cJSON_AddStringToObject(p, "subs_type", subs_type ? subs_type : "PREPAID");
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v8/xl-stores/options/search/family-list", p, id_token);
    cJSON_Delete(p);
    return r;
}

static cJSON* mk_filter(const char* unit, const char* id, const char* type, cJSON* items) {
    cJSON* f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "unit", unit);
    cJSON_AddStringToObject(f, "id", id);
    cJSON_AddStringToObject(f, "type", type);
    cJSON_AddItemToObject(f, "items", items);
    return f;
}

cJSON* store_get_packages(const char* base, const char* api_key, const char* xdata_key,
                          const char* sec, const char* id_token,
                          const char* subs_type, int is_enterprise) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", is_enterprise ? 1 : 0);
    cJSON* filters = cJSON_AddArrayToObject(p, "filters");
    cJSON_AddItemToArray(filters, mk_filter("THOUSAND", "FIL_SEL_P", "PRICE", cJSON_CreateArray()));
    cJSON_AddItemToArray(filters, mk_filter("GB", "FIL_SEL_MQ", "DATA_TYPE", cJSON_CreateArray()));
    cJSON* name_items = cJSON_CreateArray();
    cJSON* empty_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(empty_obj, "id", "");
    cJSON_AddStringToObject(empty_obj, "label", "");
    cJSON_AddItemToArray(name_items, empty_obj);
    cJSON_AddItemToArray(filters, mk_filter("PACKAGE_NAME", "FIL_PKG_N", "PACKAGE_NAME", name_items));
    cJSON_AddItemToArray(filters, mk_filter("DAY", "FIL_SEL_V", "VALIDITY", cJSON_CreateArray()));
    cJSON_AddStringToObject(p, "substype", subs_type ? subs_type : "PREPAID");
    cJSON_AddStringToObject(p, "text_search", "");
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v9/xl-stores/options/search", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_get_segments(const char* base, const char* api_key, const char* xdata_key,
                          const char* sec, const char* id_token, int is_enterprise) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", is_enterprise ? 1 : 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v8/configs/store/segments", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_get_redeemables(const char* base, const char* api_key, const char* xdata_key,
                             const char* sec, const char* id_token, int is_enterprise) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", is_enterprise ? 1 : 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v8/personalization/redeemables", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_get_notifications(const char* base, const char* api_key, const char* xdata_key,
                               const char* sec, const char* id_token) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v8/notifications", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_dashboard_segments(const char* base, const char* api_key,
                                const char* xdata_key, const char* sec,
                                const char* id_token, const char* access_token) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "access_token", access_token ? access_token : "");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "dashboard/api/v8/segments", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_get_notification_detail(const char* base, const char* api_key,
                                     const char* xdata_key, const char* sec,
                                     const char* id_token, const char* notification_id) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON_AddStringToObject(p, "notification_id", notification_id ? notification_id : "");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v8/notification/detail", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_validate_puk(const char* base, const char* api_key,
                          const char* xdata_key, const char* sec,
                          const char* msisdn, const char* puk) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "puk", puk ? puk : "");
    cJSON_AddBoolToObject(p, "is_enc", 0);
    cJSON_AddStringToObject(p, "msisdn", msisdn ? msisdn : "");
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v8/infos/validate-puk", p, "");
    cJSON_Delete(p);
    return r;
}

/* === Discovery endpoints (lihat doc di store.h) === */

cJSON* store_search_packages(const char* base, const char* api_key,
                             const char* xdata_key, const char* sec,
                             const char* id_token, const char* subs_type,
                             int is_enterprise, const char* keyword) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", is_enterprise ? 1 : 0);
    cJSON* filters = cJSON_AddArrayToObject(p, "filters");
    cJSON_AddItemToArray(filters, mk_filter("THOUSAND", "FIL_SEL_P", "PRICE", cJSON_CreateArray()));
    cJSON_AddItemToArray(filters, mk_filter("GB", "FIL_SEL_MQ", "DATA_TYPE", cJSON_CreateArray()));
    cJSON* name_items = cJSON_CreateArray();
    cJSON* empty_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(empty_obj, "id", "");
    cJSON_AddStringToObject(empty_obj, "label", "");
    cJSON_AddItemToArray(name_items, empty_obj);
    cJSON_AddItemToArray(filters, mk_filter("PACKAGE_NAME", "FIL_PKG_N", "PACKAGE_NAME", name_items));
    cJSON_AddItemToArray(filters, mk_filter("DAY", "FIL_SEL_V", "VALIDITY", cJSON_CreateArray()));
    cJSON_AddStringToObject(p, "substype", subs_type ? subs_type : "PREPAID");
    cJSON_AddStringToObject(p, "text_search", keyword ? keyword : "");
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v9/xl-stores/options/search", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_get_recommendations(const char* base, const char* api_key,
                                 const char* xdata_key, const char* sec,
                                 const char* id_token) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "api/v8/xl-stores/options/recommendation", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_get_home_banners(const char* base, const char* api_key,
                              const char* xdata_key, const char* sec,
                              const char* id_token) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "store/api/v8/banners", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_get_dynamic_banners(const char* base, const char* api_key,
                                 const char* xdata_key, const char* sec,
                                 const char* id_token,
                                 const char* placement_type) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "placement_type",
                            placement_type ? placement_type : "HOME");
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "personalization/dynamic-banners", p, id_token);
    cJSON_Delete(p);
    return r;
}

cJSON* store_get_tier_rewards(const char* base, const char* api_key,
                              const char* xdata_key, const char* sec,
                              const char* id_token) {
    cJSON* p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "is_enterprise", 0);
    cJSON_AddStringToObject(p, "lang", "en");
    cJSON* r = post(base, api_key, xdata_key, sec,
                    "gamification/api/v8/loyalties/tiering/rewards-catalog",
                    p, id_token);
    cJSON_Delete(p);
    return r;
}

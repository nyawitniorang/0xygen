#ifndef ENGSEL_CLIENT_STORE_H
#define ENGSEL_CLIENT_STORE_H

#include "../cJSON.h"

cJSON* store_get_family_list(const char* base, const char* api_key, const char* xdata_key,
                             const char* sec, const char* id_token,
                             const char* subs_type, int is_enterprise);

cJSON* store_get_packages(const char* base, const char* api_key, const char* xdata_key,
                          const char* sec, const char* id_token,
                          const char* subs_type, int is_enterprise);

cJSON* store_get_segments(const char* base, const char* api_key, const char* xdata_key,
                          const char* sec, const char* id_token, int is_enterprise);

cJSON* store_get_redeemables(const char* base, const char* api_key, const char* xdata_key,
                             const char* sec, const char* id_token, int is_enterprise);

cJSON* store_get_notifications(const char* base, const char* api_key, const char* xdata_key,
                               const char* sec, const char* id_token);

/* dashboard_segments Python: POST dashboard/api/v8/segments.
   Response berisi .data.notification.data[] yang kita pakai di menu notif. */
cJSON* store_dashboard_segments(const char* base, const char* api_key, const char* xdata_key,
                                const char* sec, const char* id_token,
                                const char* access_token);

/* Ambil detail notifikasi (mark-as-read implicit). */
cJSON* store_get_notification_detail(const char* base, const char* api_key,
                                     const char* xdata_key, const char* sec,
                                     const char* id_token, const char* notification_id);

/* PUK validasi (registration.validate_puk Python). */
cJSON* store_validate_puk(const char* base, const char* api_key,
                          const char* xdata_key, const char* sec,
                          const char* msisdn, const char* puk);

#endif

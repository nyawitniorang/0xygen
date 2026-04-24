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

/* === Discovery endpoints (paket "tersembunyi"): search/recommendation/banner/tier ===
 * Dipakai untuk expose family_code yang gak muncul di kategori store reguler.
 * Source: MyXL APK 9.1.0 — PrepaidStoreApi/OfferingApi/FunBannersApi/LoyaltyTieringApi. */

/* Search keyword (free-text). Endpoint sama dengan store_get_packages tapi
 * `text_search` di-isi user input. Response: data.results[] berisi family_code
 * (action_param=FAMILY_CODE), title, original_price, discounted_price. */
cJSON* store_search_packages(const char* base, const char* api_key,
                             const char* xdata_key, const char* sec,
                             const char* id_token, const char* subs_type,
                             int is_enterprise, const char* keyword);

/* Personalized recommendation — paket yg server "saranin" untuk akun ini.
 * POST /api/v8/xl-stores/options/recommendation, body kosong (server pakai
 * id_token utk profile). Response: data.recommendations[] grouped by category. */
cJSON* store_get_recommendations(const char* base, const char* api_key,
                                 const char* xdata_key, const char* sec,
                                 const char* id_token);

/* Banner promo home page — banyak family_code tersembunyi muncul di sini.
 * POST /store/api/v8/banners (no body). Response: data.banners[] dengan
 * action_param (family_code/option_code) + action_type (PLP/PDP). */
cJSON* store_get_home_banners(const char* base, const char* api_key,
                              const char* xdata_key, const char* sec,
                              const char* id_token);

/* Dynamic banners — placement-aware (HOME, STORE, dll). POST
 * personalization/dynamic-banners body {placement_type}. Response:
 * data[] berisi banners[] yang seringkali link ke family_code khusus
 * segment/promo. */
cJSON* store_get_dynamic_banners(const char* base, const char* api_key,
                                 const char* xdata_key, const char* sec,
                                 const char* id_token,
                                 const char* placement_type);

/* Loyalty tier rewards-catalog — paket reward khusus tier PRIO/PRIOHYBRID
 * yg gak muncul di Store reguler. POST
 * /gamification/api/v8/loyalties/tiering/rewards-catalog (no body).
 * Response: data.tiers[].rewards[] (RewardDto: code, title, price, validity). */
cJSON* store_get_tier_rewards(const char* base, const char* api_key,
                              const char* xdata_key, const char* sec,
                              const char* id_token);

#endif

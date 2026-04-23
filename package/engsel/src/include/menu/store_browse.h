#ifndef ENGSEL_MENU_STORE_BROWSE_H
#define ENGSEL_MENU_STORE_BROWSE_H

/* Menu interaktif untuk berselancar di Store:
 *   - show_store_family_list_browse: list family -> pilih -> (info family_code,
 *     user diarahkan pakai menu 4 untuk browse-and-buy).
 *   - show_store_packages_browse (search v9): list hasil "results_price_only"
 *     dengan action_type/action_param. Jika PDP, langsung purchase_flow.
 *   - show_store_segments_browse: banner per segment (A1/B2). PDP langsung,
 *     PLP tampilkan family code untuk dipakai di menu 4.
 *   - show_redeemables_browse: list per category + item redemables. PDP/PLP.
 *   - show_notification_browse: list notifikasi + baca unread.
 */

void show_store_family_list_browse(const char* base_api, const char* api_key,
                                   const char* xdata_key, const char* x_api_sec,
                                   const char* id_token,
                                   const char* subs_type);

void show_store_packages_browse(const char* base_api, const char* api_key,
                                const char* xdata_key, const char* x_api_sec,
                                const char* id_token,
                                const char* subs_type);

void show_store_segments_browse(const char* base_api, const char* api_key,
                                const char* xdata_key, const char* x_api_sec,
                                const char* id_token);

void show_redeemables_browse(const char* base_api, const char* api_key,
                             const char* xdata_key, const char* x_api_sec,
                             const char* id_token);

void show_notification_browse(const char* base_api, const char* api_key,
                              const char* xdata_key, const char* x_api_sec,
                              const char* id_token);

/* Menu 5 Python: beli by option code manual. */
void show_buy_by_option_code(void);

#endif

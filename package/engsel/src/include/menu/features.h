#ifndef ENGSEL_MENU_FEATURES_H
#define ENGSEL_MENU_FEATURES_H

/* Submenu "Fitur Lanjutan" (Circle / Family Plan / Store / Redeem).
   Dipanggil dari main.c setelah user login. */
void show_features_menu(const char* base_api,
                        const char* api_key,
                        const char* xdata_key,
                        const char* x_api_secret,
                        const char* id_token,
                        const char* access_token);

#endif

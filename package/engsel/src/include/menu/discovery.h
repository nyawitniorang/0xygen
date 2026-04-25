#ifndef ENGSEL_MENU_DISCOVERY_H
#define ENGSEL_MENU_DISCOVERY_H

/* Sub-menu "Discovery Paket Tersembunyi" — wrap 4 endpoint discovery.
 * Wired di Fitur Lanjutan item 8.
 *
 * Tujuan: ekspos family_code yang tidak muncul di Menu 7 Store reguler
 * (terutama untuk akun PRIO/PRIOHYBRID — banyak paket khusus tier yang
 * disembunyikan dari catalog standar). */
void show_discovery_menu(const char* base_api, const char* api_key,
                         const char* xdata_key, const char* x_api_secret,
                         const char* id_token);

#endif

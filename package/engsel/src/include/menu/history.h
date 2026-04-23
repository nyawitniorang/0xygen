#ifndef ENGSEL_MENU_HISTORY_H
#define ENGSEL_MENU_HISTORY_H

/* Menu "Riwayat Transaksi" — paritas show_transaction_history Python.
   Menampilkan list transaksi (title, price, tanggal WIB, metode, status). */
void show_transaction_history_menu(const char* base_api,
                                   const char* api_key,
                                   const char* xdata_key,
                                   const char* x_api_secret,
                                   const char* id_token);

/* Menu R: registrasi SIM via Dukcapil (NIK+KK). */
void show_register_menu(const char* base_api,
                        const char* api_key,
                        const char* xdata_key,
                        const char* x_api_secret,
                        const char* id_token);

/* Menu V: validate MSISDN standalone. */
void show_validate_menu(const char* base_api,
                        const char* api_key,
                        const char* xdata_key,
                        const char* x_api_secret,
                        const char* id_token);

#endif

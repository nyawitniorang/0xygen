#ifndef ENGSEL_MENU_PURCHASE_FLOW_H
#define ENGSEL_MENU_PURCHASE_FLOW_H

#include "../cJSON.h"

/* Fetch package detail berdasarkan option_code, print detail,
 * lalu panggil handle_payment_menu (6 opsi: BALANCE/Decoy/xN/E-Wallet/QRIS).
 * Cocok untuk Menu 5 (manual code), Store browse, Redeemables browse.
 * Return 1 kalau user pilih "99" (goto main), 0 kalau kembali ke caller. */
int purchase_flow_by_option_code(const char* option_code);

#endif

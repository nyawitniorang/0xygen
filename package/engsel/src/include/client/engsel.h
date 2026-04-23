#ifndef ENGSEL_H
#define ENGSEL_H
#include "../cJSON.h"

cJSON* send_api_request(const char* base_url, const char* api_key, const char* xdata_key, const char* api_secret, const char* path, cJSON* payload_dict, const char* id_token, const char* method, const char* custom_signature);
cJSON* get_profile(const char* base_url, const char* api_key, const char* xdata_key, const char* api_secret, const char* id_token, const char* access_token);
cJSON* get_balance(const char* base_url, const char* api_key, const char* xdata_key, const char* api_secret, const char* id_token);
cJSON* get_quota(const char* base_url, const char* api_key, const char* xdata_key, const char* api_secret, const char* id_token);
cJSON* get_package_detail(const char* base_url, const char* api_key, const char* xdata_key, const char* api_secret, const char* id_token, const char* option_code);
cJSON* get_addons(const char* base_url, const char* api_key, const char* xdata_key, const char* api_secret, const char* id_token, const char* option_code);
cJSON* get_family(const char* base_url, const char* api_key, const char* xdata_key, const char* api_secret, const char* id_token, const char* family_code, int is_enterprise, const char* migration_type);
cJSON* execute_balance_purchase(const char* base, const char* key, const char* xdata, const char* sec, const char* enc_key, const char* id, const char* acc, const char* opt_code, int price, const char* name, const char* conf, const char* decoy_opt_code, int decoy_price, const char* decoy_name, const char* decoy_conf, const char* pay_for, int overwrite_amount, int token_confirmation_idx);
cJSON* unsubscribe(const char* base, const char* api_key, const char* xdata_key, const char* sec, const char* id_token, const char* access_token, const char* quota_code, const char* prod_subs_type, const char* prod_domain);

// ========== TAMBAHAN BARU (E‑Wallet & QRIS) ==========

cJSON* execute_ewallet_purchase(
    const char* base_url,
    const char* api_key,
    const char* xdata_key,
    const char* api_secret,
    const char* id_token,
    const char* access_token,
    const char* opt_code,
    int price,
    const char* name,
    const char* conf,
    const char* wallet_number,
    const char* payment_method,
    const char* payment_for,
    int overwrite_amount,
    int token_confirmation_idx
);

cJSON* execute_qris_purchase(
    const char* base_url,
    const char* api_key,
    const char* xdata_key,
    const char* api_secret,
    const char* id_token,
    const char* access_token,
    const char* opt_code,
    int price,
    const char* name,
    const char* conf,
    const char* payment_for,
    int overwrite_amount,
    int token_confirmation_idx,
    char** out_transaction_id
);

cJSON* get_qris_code(
    const char* base_url,
    const char* api_key,
    const char* xdata_key,
    const char* api_secret,
    const char* id_token,
    const char* transaction_id
);

void display_qr_terminal(const char* qris_string);

// Helper base64 encode (untuk generate link QRIS)
char* base64_encode_simple(const unsigned char* data, size_t len);

// ========== TAMBAHAN BARU (Riwayat, Registrasi, Validate) ==========

/* Riwayat transaksi (POST payments/api/v8/transaction-history).
   Response: { status: "SUCCESS", data: { list: [ {title, price, timestamp,
   payment_method_label, status, payment_status, ...} ] } } */
cJSON* get_transaction_history(const char* base_url, const char* api_key,
                               const char* xdata_key, const char* api_secret,
                               const char* id_token);

/* Pending payment list. */
cJSON* get_pending_payments(const char* base_url, const char* api_key,
                            const char* xdata_key, const char* api_secret,
                            const char* id_token);

/* Registrasi SIM ke Dukcapil (NIK+KK) untuk menu R. */
cJSON* dukcapil_register(const char* base_url, const char* api_key,
                        const char* xdata_key, const char* api_secret,
                        const char* id_token,
                        const char* msisdn, const char* kk, const char* nik);

/* MSISDN validasi standalone untuk menu V. */
cJSON* validate_msisdn(const char* base_url, const char* api_key,
                       const char* xdata_key, const char* api_secret,
                       const char* id_token, const char* msisdn);

#endif

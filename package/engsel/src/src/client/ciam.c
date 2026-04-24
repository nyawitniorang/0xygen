#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include "../include/client/ciam.h"
#include "../include/client/http_client.h"
#include "../include/service/crypto_helper.h"
#include "../include/util/json_util.h"
#include "../include/util/file_util.h"

#define TZ_OFFSET_SEC (7 * 3600)

// Data dummy persis seperti di Python
static const char* DUMMY_MSISDN = "6281398370564";
static const char* DUMMY_IP      = "192.169.69.69";

static int get_random_bytes(unsigned char *buf, size_t len) {
    if (RAND_bytes(buf, (int)len) == 1) return 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    return (n == len) ? 0 : -1;
}

static void generate_uuid_v4(char *out) {
    unsigned char rand[16];
    if (get_random_bytes(rand, sizeof(rand)) != 0) {
        srandom(time(NULL));
        for (int i = 0; i < 16; i++) rand[i] = random() & 0xFF;
    }
    rand[6] = (rand[6] & 0x0F) | 0x40;
    rand[8] = (rand[8] & 0x3F) | 0x80;

    sprintf(out, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            rand[0], rand[1], rand[2], rand[3],
            rand[4], rand[5],
            rand[6], rand[7],
            rand[8], rand[9],
            rand[10], rand[11], rand[12], rand[13], rand[14], rand[15]);
}

static char* md5_hex(const char *input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input, strlen(input), digest);
    char *hex = malloc(MD5_DIGEST_LENGTH * 2 + 1);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(&hex[i*2], "%02x", digest[i]);
    return hex;
}

static char* get_timestamp_with_ms(int offset_seconds) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec + offset_seconds;
    int ms = tv.tv_usec / 1000;
    struct tm *t = gmtime(&now);
    char buf[64];
    int tz_hour = offset_seconds / 3600;
    int tz_min = (offset_seconds % 3600) / 60;
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03d%+03d%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec, ms,
             tz_hour, tz_min);
    return strdup(buf);
}

static char* get_ts_for_signature(void) {
    return get_timestamp_with_ms(TZ_OFFSET_SEC);
}

static char* get_timestamp_header(void) {
    return get_timestamp_with_ms(TZ_OFFSET_SEC - 300);
}

/* Fingerprint MUST be stable across sessions — server memvalidasi fingerprint
 * saat OTP submit terhadap fingerprint saat OTP request. Kalau berubah per-call,
 * submit OTP akan ditolak walau kode benar.
 * Implementasi: cache ke /etc/engsel/ax.fp (mirror Python load_ax_fp). */
#define AX_FP_PATH "/etc/engsel/ax.fp"

static char* generate_ax_fingerprint(void) {
    /* 1) Cache lookup dulu. */
    char* cached = file_read_all(AX_FP_PATH, NULL);
    if (cached) {
        /* trim whitespace (file_read_all sudah null-terminate) */
        size_t cl = strlen(cached);
        while (cl > 0 && (cached[cl-1] == '\n' || cached[cl-1] == '\r' ||
                          cached[cl-1] == ' '  || cached[cl-1] == '\t')) {
            cached[--cl] = '\0';
        }
        if (cl > 0) return cached;   /* kembalikan cache */
        free(cached);
    }

    const char* key_str = getenv("AX_FP_KEY");
    if (!key_str) return strdup("dummy");

    unsigned int r[2];
    if (get_random_bytes((unsigned char*)r, sizeof(r)) != 0) {
        r[0] = (unsigned int)time(NULL);
        r[1] = (unsigned int)(time(NULL) ^ 0x5A5A5A5A);
    }
    int rand1 = (int)(r[0] % 9000) + 1000;
    int rand2 = (int)(r[1] % 9000) + 1000;

    char plain[512];
    snprintf(plain, sizeof(plain),
        "samsung%d|SM-N93%d|en|720x1540|GMT07:00|%s|1.0|Android 13|%s",
        rand1, rand2, DUMMY_IP, DUMMY_MSISDN);

    unsigned char key[32] = {0};
    size_t klen = strlen(key_str);
    memcpy(key, key_str, klen > 32 ? 32 : klen);
    unsigned char iv[16] = {0};

    int plain_len = strlen(plain);
    int pad = 16 - (plain_len % 16);
    int padded_len = plain_len + pad;
    unsigned char padded[512] = {0};
    memcpy(padded, plain, plain_len);
    for (int i = plain_len; i < padded_len; i++) padded[i] = (unsigned char)pad;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    unsigned char ct[512]; int len1 = 0, len2 = 0;
    EVP_EncryptUpdate(ctx, ct, &len1, padded, padded_len);
    EVP_EncryptFinal_ex(ctx, ct + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);

    int total = len1 + len2;
    char* b64 = malloc((size_t)total * 2 + 10);
    if (!b64) return strdup("dummy");
    EVP_EncodeBlock((unsigned char*)b64, ct, total);

    /* 2) Simpan ke cache agar submit_otp pakai value yang sama. */
    (void)file_write_atomic(AX_FP_PATH, b64);
    return b64;
}

static char* generate_ax_device_id(void) {
    char* fp = generate_ax_fingerprint();
    char* dev_id = md5_hex(fp);
    free(fp);
    return dev_id;
}

static char* generate_ax_api_signature(const char* ts_for_sign, const char* contact,
                                       const char* code, const char* contact_type, const char* key_str) {
    if (!key_str) return strdup("dummy");
    char *sig = make_ax_api_signature(key_str, ts_for_sign, contact, code, contact_type);
    return sig ? sig : strdup("dummy");
}

static char* extend_session(const char* base_ciam_url, const char* basic_auth, const char* ua, const char* subscriber_id) {
    char b64_subscriber_id[256];
    EVP_EncodeBlock((unsigned char*)b64_subscriber_id, (unsigned char*)subscriber_id, strlen(subscriber_id));
    char* nl = strchr(b64_subscriber_id, '\n');
    if (nl) *nl = '\0';

    char url[512];
    snprintf(url, sizeof(url), "%s/realms/xl-ciam/auth/extend-session", base_ciam_url);

    char auth_hdr[512];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Basic %s", basic_auth);
    char ua_hdr[512];
    snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: %s", ua);
    char* fp = generate_ax_fingerprint();
    char fp_hdr[1024];
    snprintf(fp_hdr, sizeof(fp_hdr), "Ax-Fingerprint: %s", fp);
    char* dev_id = generate_ax_device_id();
    char dev_id_hdr[256];
    snprintf(dev_id_hdr, sizeof(dev_id_hdr), "Ax-Device-Id: %s", dev_id);
    char* ts_hdr = get_timestamp_header();
    char req_at[128];
    snprintf(req_at, sizeof(req_at), "Ax-Request-At: %s", ts_hdr);
    free(ts_hdr);
    char req_id[64];
    generate_uuid_v4(req_id);
    char req_id_hdr[128];
    snprintf(req_id_hdr, sizeof(req_id_hdr), "Ax-Request-Id: %s", req_id);

    const char* headers[] = {
        auth_hdr, ua_hdr,
        "Ax-Request-Device: samsung",
        "Ax-Request-Device-Model: SM-N935F",
        "Ax-Substype: PREPAID",
        fp_hdr, dev_id_hdr, req_at, req_id_hdr
    };

    char params[512];
    snprintf(params, sizeof(params), "contact=%s&contactType=DEVICEID", b64_subscriber_id);
    char full_url[1024];
    snprintf(full_url, sizeof(full_url), "%s?%s", url, params);

    struct HttpResponse* response = http_get(full_url, headers, 9);
    char* exchange_code = NULL;
    if (response && response->body) {
        cJSON* json = cJSON_Parse(response->body);
        if (json) {
            cJSON* data = cJSON_GetObjectItem(json, "data");
            if (data) {
                cJSON* ec = cJSON_GetObjectItem(data, "exchange_code");
                if (ec && cJSON_IsString(ec))
                    exchange_code = strdup(ec->valuestring);
            }
            cJSON_Delete(json);
        }
    }
    free_http_response(response);
    free(fp);
    free(dev_id);
    return exchange_code;
}

cJSON* get_new_token(const char* base_ciam_url, const char* basic_auth, const char* ua,
                     const char* refresh_token, const char* subscriber_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/realms/xl-ciam/protocol/openid-connect/token", base_ciam_url);
    char payload[2048];
    snprintf(payload, sizeof(payload), "grant_type=refresh_token&refresh_token=%s", refresh_token);
    char auth_hdr[512];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Basic %s", basic_auth);
    char ua_hdr[512];
    snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: %s", ua);
    char* fp = generate_ax_fingerprint();
    char fp_hdr[1024];
    snprintf(fp_hdr, sizeof(fp_hdr), "Ax-Fingerprint: %s", fp);
    char* dev_id = generate_ax_device_id();
    char dev_id_hdr[256];
    snprintf(dev_id_hdr, sizeof(dev_id_hdr), "Ax-Device-Id: %s", dev_id);
    char* ts_hdr = get_timestamp_header();
    char req_at[128];
    snprintf(req_at, sizeof(req_at), "Ax-Request-At: %s", ts_hdr);
    free(ts_hdr);
    char req_id[64];
    generate_uuid_v4(req_id);
    char req_id_hdr[128];
    snprintf(req_id_hdr, sizeof(req_id_hdr), "Ax-Request-Id: %s", req_id);

    const char* headers[] = {
        "Content-Type: application/x-www-form-urlencoded",
        "Ax-Request-Device: samsung",
        "Ax-Request-Device-Model: SM-N935F",
        "Ax-Substype: PREPAID",
        fp_hdr, dev_id_hdr, req_at, req_id_hdr,
        auth_hdr, ua_hdr
    };
    struct HttpResponse* response = http_post(url, headers, 10, payload);
    
    if (response) {
        if (response->status_code == 400 && response->body) {
            cJSON* err_json = cJSON_Parse(response->body);
            if (err_json) {
                cJSON* err_desc = cJSON_GetObjectItem(err_json, "error_description");
                if (err_desc && cJSON_IsString(err_desc) && strstr(err_desc->valuestring, "Session not active")) {
                    cJSON_Delete(err_json);
                    free_http_response(response);
                    if (subscriber_id && strlen(subscriber_id) > 0) {
                        char* exchange_code = extend_session(base_ciam_url, basic_auth, ua, subscriber_id);
                        if (exchange_code) {
                            const char* ax_key = getenv("AX_API_SIG_KEY");
                            cJSON* new_tokens = submit_otp(base_ciam_url, basic_auth, ua,
                                                           ax_key ? ax_key : "dummy",
                                                           "DEVICEID", subscriber_id, exchange_code);
                            free(exchange_code);
                            free(fp); free(dev_id);
                            return new_tokens;
                        }
                    }
                } else {
                    cJSON_Delete(err_json);
                }
            }
        }
        cJSON* result = NULL;
        if (response->body) result = cJSON_Parse(response->body);
        free_http_response(response);
        free(fp); free(dev_id);
        return result;
    }
    free(fp); free(dev_id);
    return NULL;
}

cJSON* request_otp(const char* base_ciam_url, const char* basic_auth, const char* ua,
                   const char* number) {
    char url[512];
    snprintf(url, sizeof(url), "%s/realms/xl-ciam/auth/otp?contact=%s&contactType=SMS&alternateContact=false",
             base_ciam_url, number);
    char auth_hdr[512];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Basic %s", basic_auth);
    char ua_hdr[512];
    snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: %s", ua);
    char* fp = generate_ax_fingerprint();
    char fp_hdr[1024];
    snprintf(fp_hdr, sizeof(fp_hdr), "Ax-Fingerprint: %s", fp);
    char* dev_id = generate_ax_device_id();
    char dev_id_hdr[256];
    snprintf(dev_id_hdr, sizeof(dev_id_hdr), "Ax-Device-Id: %s", dev_id);
    char* ts_hdr = get_timestamp_header();
    char req_at[128];
    snprintf(req_at, sizeof(req_at), "Ax-Request-At: %s", ts_hdr);
    free(ts_hdr);
    char req_id[64];
    generate_uuid_v4(req_id);
    char req_id_hdr[128];
    snprintf(req_id_hdr, sizeof(req_id_hdr), "Ax-Request-Id: %s", req_id);

    const char* headers[] = {
        auth_hdr, ua_hdr,
        "Ax-Request-Device: samsung",
        "Ax-Request-Device-Model: SM-N935F",
        "Ax-Substype: PREPAID",
        fp_hdr, dev_id_hdr, req_at, req_id_hdr
    };
    struct HttpResponse* response = http_get(url, headers, 9);
    cJSON* result = NULL;
    if (response && response->body && strlen(response->body) > 0)
        result = cJSON_Parse(response->body);
    free_http_response(response);
    free(fp);
    free(dev_id);
    return result;
}

cJSON* submit_otp(const char* base_ciam_url, const char* basic_auth, const char* ua,
                  const char* ax_api_sig_key, const char* contact_type, const char* contact, const char* code) {
    char url[512];
    snprintf(url, sizeof(url), "%s/realms/xl-ciam/protocol/openid-connect/token", base_ciam_url);

    char final_contact[512];
    if (strcmp(contact_type, "DEVICEID") == 0) {
        EVP_EncodeBlock((unsigned char*)final_contact, (unsigned char*)contact, strlen(contact));
        char* nl = strchr(final_contact, '\n');
        if (nl) *nl = '\0';
    } else {
        strncpy(final_contact, contact, sizeof(final_contact)-1);
    }

    char payload[1024];
    snprintf(payload, sizeof(payload),
             "contactType=%s&code=%s&grant_type=password&contact=%s&scope=openid",
             contact_type, code, final_contact);

    char* ts_for_sign = get_ts_for_signature();
    char* signature = generate_ax_api_signature(ts_for_sign, final_contact, code, contact_type, ax_api_sig_key);

    char auth_hdr[512];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Basic %s", basic_auth);
    char ua_hdr[512];
    snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: %s", ua);
    char sig_hdr[512];
    snprintf(sig_hdr, sizeof(sig_hdr), "Ax-Api-Signature: %s", signature);
    char* fp = generate_ax_fingerprint();
    char fp_hdr[1024];
    snprintf(fp_hdr, sizeof(fp_hdr), "Ax-Fingerprint: %s", fp);
    char* dev_id = generate_ax_device_id();
    char dev_id_hdr[256];
    snprintf(dev_id_hdr, sizeof(dev_id_hdr), "Ax-Device-Id: %s", dev_id);
    char* ts_hdr = get_timestamp_header();
    char req_at[128];
    snprintf(req_at, sizeof(req_at), "Ax-Request-At: %s", ts_hdr);
    free(ts_hdr);
    char req_id[64];
    generate_uuid_v4(req_id);
    char req_id_hdr[128];
    snprintf(req_id_hdr, sizeof(req_id_hdr), "Ax-Request-Id: %s", req_id);

    const char* headers[] = {
        "Content-Type: application/x-www-form-urlencoded",
        auth_hdr, ua_hdr, sig_hdr,
        "Ax-Request-Device: samsung",
        "Ax-Request-Device-Model: SM-N935F",
        "Ax-Substype: PREPAID",
        fp_hdr, dev_id_hdr, req_at, req_id_hdr
    };

    struct HttpResponse* response = http_post(url, headers, 11, payload);
    cJSON* result = NULL;
    if (response && response->body && strlen(response->body) > 0)
        result = cJSON_Parse(response->body);
    free_http_response(response);
    free(ts_for_sign);
    free(signature);
    free(fp);
    free(dev_id);
    return result;
}

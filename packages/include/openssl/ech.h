




#ifndef OPENSSL_ECH_H
#define OPENSSL_ECH_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/ssl.h>
#include <openssl/hpke.h>

#ifndef OPENSSL_NO_ECH




#define OSSL_ECH_MAX_PAYLOAD_LEN 1500
#define OSSL_ECH_MIN_ECHCONFIG_LEN 32
#define OSSL_ECH_MAX_ECHCONFIG_LEN 1500
#define OSSL_ECH_MAX_ECHCONFIGEXT_LEN 512
#define OSSL_ECH_MAX_MAXNAMELEN 255
#define OSSL_ECH_MAX_PUBLICNAME 255
#define OSSL_ECH_MAX_ALPNLEN 255
#define OSSL_ECH_OUTERS_MAX 20
#define OSSL_ECH_ALLEXTS_MAX 32




#define OSSL_ECH_RFC9849_VERSION 0xfe0d

#define OSSL_ECH_CURRENT_VERSION OSSL_ECH_RFC9849_VERSION


#define SSL_ECH_STATUS_BACKEND 4
#define SSL_ECH_STATUS_GREASE_ECH 3
#define SSL_ECH_STATUS_GREASE 2
#define SSL_ECH_STATUS_SUCCESS 1
#define SSL_ECH_STATUS_FAILED 0
#define SSL_ECH_STATUS_BAD_CALL -100
#define SSL_ECH_STATUS_NOT_TRIED -101
#define SSL_ECH_STATUS_BAD_NAME -102
#define SSL_ECH_STATUS_NOT_CONFIGURED -103
#define SSL_ECH_STATUS_FAILED_ECH -105
#define SSL_ECH_STATUS_FAILED_ECH_BAD_NAME -106


#define OSSL_ECHSTORE_LAST -1

#define OSSL_ECHSTORE_ALL -2


#define OSSL_ECH_FOR_RETRY 1
#define OSSL_ECH_NO_RETRY 0




OSSL_ECHSTORE *OSSL_ECHSTORE_new(OSSL_LIB_CTX *libctx, const char *propq);
void OSSL_ECHSTORE_free(OSSL_ECHSTORE *es);
int OSSL_ECHSTORE_new_config(OSSL_ECHSTORE *es,
    uint16_t echversion, uint8_t max_name_length,
    const char *public_name, OSSL_HPKE_SUITE suite);
int OSSL_ECHSTORE_write_pem(OSSL_ECHSTORE *es, int index, BIO *out);
int OSSL_ECHSTORE_read_echconfiglist(OSSL_ECHSTORE *es, BIO *in);
int OSSL_ECHSTORE_get1_info(OSSL_ECHSTORE *es, int index, time_t *loaded_secs,
    char **public_name, char **echconfig,
    int *has_private, int *for_retry);
int OSSL_ECHSTORE_downselect(OSSL_ECHSTORE *es, int index);
int OSSL_ECHSTORE_set1_key_and_read_pem(OSSL_ECHSTORE *es, EVP_PKEY *priv,
    BIO *in, int for_retry);
int OSSL_ECHSTORE_read_pem(OSSL_ECHSTORE *es, BIO *in, int for_retry);
int OSSL_ECHSTORE_num_entries(const OSSL_ECHSTORE *es, int *numentries);
int OSSL_ECHSTORE_num_keys(OSSL_ECHSTORE *es, int *numkeys);
int OSSL_ECHSTORE_flush_keys(OSSL_ECHSTORE *es, time_t age);




int SSL_CTX_set1_echstore(SSL_CTX *ctx, OSSL_ECHSTORE *es);
int SSL_set1_echstore(SSL *s, OSSL_ECHSTORE *es);

OSSL_ECHSTORE *SSL_CTX_get1_echstore(const SSL_CTX *ctx);
OSSL_ECHSTORE *SSL_get1_echstore(const SSL *s);

int SSL_ech_set1_server_names(SSL *s, const char *inner_name,
    const char *outer_name, int no_outer);
int SSL_ech_set1_outer_server_name(SSL *s, const char *outer_name, int no_outer);




int SSL_ech_set1_outer_alpn_protos(SSL *s, const unsigned char *protos,
    const size_t protos_len);

int SSL_ech_get1_status(SSL *s, char **inner_sni, char **outer_sni);
int SSL_ech_set1_grease_suite(SSL *s, const char *suite);
int SSL_ech_set_grease_type(SSL *s, uint16_t type);
typedef unsigned int (*SSL_ech_cb_func)(SSL *s, const char *str);
void SSL_ech_set_callback(SSL *s, SSL_ech_cb_func f);
int SSL_ech_get1_retry_config(SSL *s, unsigned char **ec, size_t *eclen);




int SSL_CTX_ech_set1_outer_alpn_protos(SSL_CTX *s, const unsigned char *protos,
    const size_t protos_len);
void SSL_CTX_ech_set_callback(SSL_CTX *ctx, SSL_ech_cb_func f);
int SSL_set1_ech_config_list(SSL *ssl, const uint8_t *ecl, size_t ecl_len);

#ifdef __cplusplus
}
#endif

#endif
#endif

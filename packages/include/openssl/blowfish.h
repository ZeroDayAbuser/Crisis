

#ifndef OPENSSL_BLOWFISH_H
#define OPENSSL_BLOWFISH_H
#pragma once

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_BLOWFISH_H
#endif

#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_BF
#include <openssl/e_os2.h>
#ifdef __cplusplus
extern "C" {
#endif

#define BF_BLOCK 8

#ifndef OPENSSL_NO_DEPRECATED_3_0

#define BF_ENCRYPT 1
#define BF_DECRYPT 0


typedef unsigned int BF_LONG;

#define BF_ROUNDS 16

typedef struct bf_key_st {
    BF_LONG P[BF_ROUNDS + 2];
    BF_LONG S[4 * 256];
} BF_KEY;

#endif 
#ifndef OPENSSL_NO_DEPRECATED_3_0
OSSL_DEPRECATEDIN_3_0 void BF_set_key(BF_KEY *key, int len,
    const unsigned char *data);
OSSL_DEPRECATEDIN_3_0 void BF_encrypt(BF_LONG *data, const BF_KEY *key);
OSSL_DEPRECATEDIN_3_0 void BF_decrypt(BF_LONG *data, const BF_KEY *key);
OSSL_DEPRECATEDIN_3_0 void BF_ecb_encrypt(const unsigned char *in,
    unsigned char *out, const BF_KEY *key,
    int enc);
OSSL_DEPRECATEDIN_3_0 void BF_cbc_encrypt(const unsigned char *in,
    unsigned char *out, long length,
    const BF_KEY *schedule,
    unsigned char *ivec, int enc);
OSSL_DEPRECATEDIN_3_0 void BF_cfb64_encrypt(const unsigned char *in,
    unsigned char *out,
    long length, const BF_KEY *schedule,
    unsigned char *ivec, int *num,
    int enc);
OSSL_DEPRECATEDIN_3_0 void BF_ofb64_encrypt(const unsigned char *in,
    unsigned char *out,
    long length, const BF_KEY *schedule,
    unsigned char *ivec, int *num);
OSSL_DEPRECATEDIN_3_0 const char *BF_options(void);
#endif

#ifdef __cplusplus
}
#endif
#endif

#endif

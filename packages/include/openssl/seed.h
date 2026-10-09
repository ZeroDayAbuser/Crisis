

































#ifndef OPENSSL_SEED_H
#define OPENSSL_SEED_H
#pragma once

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_SEED_H
#endif

#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_SEED
#include <openssl/e_os2.h>
#include <openssl/crypto.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEED_BLOCK_SIZE 16
#define SEED_KEY_LENGTH 16

#ifndef OPENSSL_NO_DEPRECATED_3_0

#ifdef AES_LONG
#ifndef SEED_LONG
#define SEED_LONG 1
#endif
#endif

typedef struct seed_key_st {
#ifdef SEED_LONG
    unsigned long data[32];
#else
    unsigned int data[32];
#endif
} SEED_KEY_SCHEDULE;
#endif 
#ifndef OPENSSL_NO_DEPRECATED_3_0
OSSL_DEPRECATEDIN_3_0
void SEED_set_key(const unsigned char rawkey[SEED_KEY_LENGTH],
    SEED_KEY_SCHEDULE *ks);
OSSL_DEPRECATEDIN_3_0
void SEED_encrypt(const unsigned char s[SEED_BLOCK_SIZE],
    unsigned char d[SEED_BLOCK_SIZE],
    const SEED_KEY_SCHEDULE *ks);
OSSL_DEPRECATEDIN_3_0
void SEED_decrypt(const unsigned char s[SEED_BLOCK_SIZE],
    unsigned char d[SEED_BLOCK_SIZE],
    const SEED_KEY_SCHEDULE *ks);
OSSL_DEPRECATEDIN_3_0
void SEED_ecb_encrypt(const unsigned char *in,
    unsigned char *out,
    const SEED_KEY_SCHEDULE *ks, int enc);
OSSL_DEPRECATEDIN_3_0
void SEED_cbc_encrypt(const unsigned char *in, unsigned char *out, size_t len,
    const SEED_KEY_SCHEDULE *ks,
    unsigned char ivec[SEED_BLOCK_SIZE],
    int enc);
OSSL_DEPRECATEDIN_3_0
void SEED_cfb128_encrypt(const unsigned char *in, unsigned char *out,
    size_t len, const SEED_KEY_SCHEDULE *ks,
    unsigned char ivec[SEED_BLOCK_SIZE],
    int *num, int enc);
OSSL_DEPRECATEDIN_3_0
void SEED_ofb128_encrypt(const unsigned char *in, unsigned char *out,
    size_t len, const SEED_KEY_SCHEDULE *ks,
    unsigned char ivec[SEED_BLOCK_SIZE],
    int *num);
#endif

#ifdef __cplusplus
}
#endif
#endif

#endif

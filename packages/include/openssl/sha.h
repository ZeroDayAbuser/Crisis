








#ifndef OPENSSL_SHA_H
#define OPENSSL_SHA_H
#pragma once

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_SHA_H
#endif

#include <openssl/e_os2.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA_DIGEST_LENGTH 20

#ifndef OPENSSL_NO_DEPRECATED_3_0





typedef unsigned int SHA_LONG;

#define SHA_LBLOCK 16
#define SHA_CBLOCK (SHA_LBLOCK * 4) 


#define SHA_LAST_BLOCK (SHA_CBLOCK - 8)

typedef struct SHAstate_st {
    SHA_LONG h0, h1, h2, h3, h4;
    SHA_LONG Nl, Nh;
    SHA_LONG data[SHA_LBLOCK];
    unsigned int num;
} SHA_CTX;

OSSL_DEPRECATEDIN_3_0 int SHA1_Init(SHA_CTX *c);
OSSL_DEPRECATEDIN_3_0 int SHA1_Update(SHA_CTX *c, const void *data, size_t len);
OSSL_DEPRECATEDIN_3_0 int SHA1_Final(unsigned char *md, SHA_CTX *c);
OSSL_DEPRECATEDIN_3_0 void SHA1_Transform(SHA_CTX *c, const unsigned char *data);
#endif

unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md);

#ifndef OPENSSL_NO_DEPRECATED_3_0
#define SHA256_CBLOCK (SHA_LBLOCK * 4) 



typedef struct SHA256state_st {
    SHA_LONG h[8]; 
    SHA_LONG Nl, Nh; 
    SHA_LONG data[SHA_LBLOCK]; 
    unsigned int num; 
    unsigned int md_len; 
} SHA256_CTX;

OSSL_DEPRECATEDIN_3_0 int SHA224_Init(SHA256_CTX *c);
OSSL_DEPRECATEDIN_3_0 int SHA224_Update(SHA256_CTX *c,
    const void *data, size_t len);
OSSL_DEPRECATEDIN_3_0 int SHA224_Final(unsigned char *md, SHA256_CTX *c);
OSSL_DEPRECATEDIN_3_0 int SHA256_Init(SHA256_CTX *c);
OSSL_DEPRECATEDIN_3_0 int SHA256_Update(SHA256_CTX *c,
    const void *data, size_t len);
OSSL_DEPRECATEDIN_3_0 int SHA256_Final(unsigned char *md, SHA256_CTX *c);
OSSL_DEPRECATEDIN_3_0 void SHA256_Transform(SHA256_CTX *c,
    const unsigned char *data);
#endif

unsigned char *SHA224(const unsigned char *d, size_t n, unsigned char *md);
unsigned char *SHA256(const unsigned char *d, size_t n, unsigned char *md);

#define SHA256_192_DIGEST_LENGTH 24
#define SHA224_DIGEST_LENGTH 28
#define SHA256_DIGEST_LENGTH 32
#define SHA384_DIGEST_LENGTH 48
#define SHA512_DIGEST_LENGTH 64

#ifndef OPENSSL_NO_DEPRECATED_3_0










#define SHA512_CBLOCK (SHA_LBLOCK * 8)
#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
typedef unsigned __int64 SHA_LONG64;
#elif defined(__arch64__)
typedef unsigned long SHA_LONG64;
#else
typedef unsigned long long SHA_LONG64;
#endif

typedef struct SHA512state_st {
    SHA_LONG64 h[8];
    SHA_LONG64 Nl, Nh;
    union {
        SHA_LONG64 d[SHA_LBLOCK];
        unsigned char p[SHA512_CBLOCK];
    } u;
    unsigned int num, md_len;
} SHA512_CTX;

OSSL_DEPRECATEDIN_3_0 int SHA384_Init(SHA512_CTX *c);
OSSL_DEPRECATEDIN_3_0 int SHA384_Update(SHA512_CTX *c,
    const void *data, size_t len);
OSSL_DEPRECATEDIN_3_0 int SHA384_Final(unsigned char *md, SHA512_CTX *c);
OSSL_DEPRECATEDIN_3_0 int SHA512_Init(SHA512_CTX *c);
OSSL_DEPRECATEDIN_3_0 int SHA512_Update(SHA512_CTX *c,
    const void *data, size_t len);
OSSL_DEPRECATEDIN_3_0 int SHA512_Final(unsigned char *md, SHA512_CTX *c);
OSSL_DEPRECATEDIN_3_0 void SHA512_Transform(SHA512_CTX *c,
    const unsigned char *data);
#endif

unsigned char *SHA384(const unsigned char *d, size_t n, unsigned char *md);
unsigned char *SHA512(const unsigned char *d, size_t n, unsigned char *md);

#ifdef __cplusplus
}
#endif

#endif

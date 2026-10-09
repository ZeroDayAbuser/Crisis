








#ifndef OPENSSL_TRACE_H
#define OPENSSL_TRACE_H
#pragma once

#include <stdarg.h>

#include <openssl/bio.h>

#ifdef __cplusplus
extern "C" {
#endif



















#define OSSL_TRACE_CATEGORY_ALL 0 
#define OSSL_TRACE_CATEGORY_TRACE 1
#define OSSL_TRACE_CATEGORY_INIT 2
#define OSSL_TRACE_CATEGORY_TLS 3
#define OSSL_TRACE_CATEGORY_TLS_CIPHER 4
#define OSSL_TRACE_CATEGORY_CONF 5
#define OSSL_TRACE_CATEGORY_PKCS5V2 6
#define OSSL_TRACE_CATEGORY_PKCS12_KEYGEN 7
#define OSSL_TRACE_CATEGORY_PKCS12_DECRYPT 8
#define OSSL_TRACE_CATEGORY_X509V3_POLICY 9
#define OSSL_TRACE_CATEGORY_BN_CTX 10
#define OSSL_TRACE_CATEGORY_CMP 11
#define OSSL_TRACE_CATEGORY_STORE 12
#define OSSL_TRACE_CATEGORY_DECODER 13
#define OSSL_TRACE_CATEGORY_ENCODER 14
#define OSSL_TRACE_CATEGORY_REF_COUNT 15
#define OSSL_TRACE_CATEGORY_HTTP 16
#define OSSL_TRACE_CATEGORY_PROVIDER 17
#define OSSL_TRACE_CATEGORY_QUERY 18
#define OSSL_TRACE_CATEGORY_NUM 19



int OSSL_trace_get_category_num(const char *name);


const char *OSSL_trace_get_category_name(int num);












int OSSL_trace_set_channel(int category, BIO *channel);










int OSSL_trace_set_prefix(int category, const char *prefix);
int OSSL_trace_set_suffix(int category, const char *suffix);












typedef size_t (*OSSL_trace_cb)(const char *buffer, size_t count,
    int category, int cmd, void *data);



#define OSSL_TRACE_CTRL_BEGIN 0
#define OSSL_TRACE_CTRL_WRITE 1
#define OSSL_TRACE_CTRL_END 2












int OSSL_trace_set_callback(int category, OSSL_trace_cb callback, void *data);








int OSSL_trace_enabled(int category);


















BIO *OSSL_trace_begin(int category);
void OSSL_trace_end(int category, BIO *channel);














































#if !defined OPENSSL_NO_TRACE && !defined FIPS_MODULE

#define OSSL_TRACE_BEGIN(category)                                       \
    do {                                                                 \
        BIO *trc_out = OSSL_trace_begin(OSSL_TRACE_CATEGORY_##category); \
                                                                         \
        if (trc_out != NULL)

#define OSSL_TRACE_END(category)                             \
    OSSL_trace_end(OSSL_TRACE_CATEGORY_##category, trc_out); \
    }                                                        \
    while (0)

#define OSSL_TRACE_CANCEL(category) \
    OSSL_trace_end(OSSL_TRACE_CATEGORY_##category, trc_out)

#else

#define OSSL_TRACE_BEGIN(category) \
    do {                           \
        BIO *trc_out = NULL;       \
        if (0)

#define OSSL_TRACE_END(category) \
    }                            \
    while (0)

#define OSSL_TRACE_CANCEL(category) \
    ((void)0)

#endif










#if !defined OPENSSL_NO_TRACE && !defined FIPS_MODULE

#define OSSL_TRACE_ENABLED(category) \
    OSSL_trace_enabled(OSSL_TRACE_CATEGORY_##category)

#else

#define OSSL_TRACE_ENABLED(category) (0)

#endif























#if !defined OPENSSL_NO_TRACE && !defined FIPS_MODULE

#define OSSL_TRACEV(category, args) \
    OSSL_TRACE_BEGIN(category)      \
    BIO_printf args;                \
    OSSL_TRACE_END(category)

#else

#define OSSL_TRACEV(category, args) ((void)0)

#endif

#define OSSL_TRACE(category, text) \
    OSSL_TRACEV(category, (trc_out, "%s", text))

#define OSSL_TRACE1(category, format, arg1) \
    OSSL_TRACEV(category, (trc_out, format, arg1))
#define OSSL_TRACE2(category, format, arg1, arg2) \
    OSSL_TRACEV(category, (trc_out, format, arg1, arg2))
#define OSSL_TRACE3(category, format, arg1, arg2, arg3) \
    OSSL_TRACEV(category, (trc_out, format, arg1, arg2, arg3))
#define OSSL_TRACE4(category, format, arg1, arg2, arg3, arg4) \
    OSSL_TRACEV(category, (trc_out, format, arg1, arg2, arg3, arg4))
#define OSSL_TRACE5(category, format, arg1, arg2, arg3, arg4, arg5) \
    OSSL_TRACEV(category, (trc_out, format, arg1, arg2, arg3, arg4, arg5))
#define OSSL_TRACE6(category, format, arg1, arg2, arg3, arg4, arg5, arg6) \
    OSSL_TRACEV(category, (trc_out, format, arg1, arg2, arg3, arg4, arg5, arg6))
#define OSSL_TRACE7(category, format, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    OSSL_TRACEV(category, (trc_out, format, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define OSSL_TRACE8(category, format, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    OSSL_TRACEV(category, (trc_out, format, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define OSSL_TRACE9(category, format, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    OSSL_TRACEV(category, (trc_out, format, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))

#define OSSL_TRACE_STRING_MAX 80
int OSSL_trace_string(BIO *out, int text, int full,
    const unsigned char *data, size_t size);
#define OSSL_TRACE_STRING(category, text, full, data, len) \
    OSSL_TRACE_BEGIN(category)                             \
    {                                                      \
        OSSL_trace_string(trc_out, text, full, data, len); \
    }                                                      \
    OSSL_TRACE_END(category)

#ifdef __cplusplus
}
#endif

#endif

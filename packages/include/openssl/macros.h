

#ifndef OPENSSL_MACROS_H
#define OPENSSL_MACROS_H
#pragma once

#include <openssl/opensslconf.h>
#include <openssl/opensslv.h>


#define OPENSSL_MSTR_HELPER(x) #x
#define OPENSSL_MSTR(x) OPENSSL_MSTR_HELPER(x)


#define NON_EMPTY_TRANSLATION_UNIT static void *dummy = &dummy;


#ifndef OSSL_DEPRECATED
#undef OSSL_DEPRECATED_FOR
#ifndef OPENSSL_SUPPRESS_DEPRECATED
#if defined(_MSC_VER)

#if _MSC_VER >= 1400
#define OSSL_DEPRECATED(since) \
    __declspec(deprecated("Since OpenSSL " #since))
#define OSSL_DEPRECATED_FOR(since, message) \
    __declspec(deprecated("Since OpenSSL " #since ";" message))
#define OSSL_DEPRECATED_MESSAGE(message) __declspec(deprecated(message))
#elif _MSC_VER >= 1310
#define OSSL_DEPRECATED(since) __declspec(deprecated)
#define OSSL_DEPRECATED_FOR(since, message) __declspec(deprecated)
#define OSSL_DEPRECATED_MESSAGE(message) __declspec(deprecated)
#endif
#define OSSL_BEGIN_ALLOW_DEPRECATED \
    __pragma(warning(push)) __pragma(warning(disable : 4996))
#define OSSL_END_ALLOW_DEPRECATED __pragma(warning(pop))
#elif defined(__GNUC__)

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define OSSL_DEPRECATED(since) \
    __attribute__((deprecated("Since OpenSSL " #since)))
#define OSSL_DEPRECATED_FOR(since, message) \
    __attribute__((deprecated("Since OpenSSL " #since ";" message)))
#define OSSL_DEPRECATED_MESSAGE(message) __attribute__((deprecated(message)))
#elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 0)
#define OSSL_DEPRECATED(since) __attribute__((deprecated))
#define OSSL_DEPRECATED_FOR(since, message) __attribute__((deprecated))
#define OSSL_DEPRECATED_MESSAGE(message) __attribute__((deprecated))
#endif
#define OSSL_BEGIN_ALLOW_DEPRECATED \
    _Pragma("GCC diagnostic push")  \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define OSSL_END_ALLOW_DEPRECATED _Pragma("GCC diagnostic pop")
#elif defined(__SUNPRO_C)
#if (__SUNPRO_C >= 0x5130)
#define OSSL_DEPRECATED(since) __attribute__((deprecated))
#define OSSL_DEPRECATED_FOR(since, message) __attribute__((deprecated))
#define OSSL_DEPRECATED_MESSAGE(message) __attribute__((deprecated))
#endif
#define OSSL_BEGIN_ALLOW_DEPRECATED \
    #pragma error_messages(off, E_DEPRECATED_ATT, E_DEPRECATED_ATT_MESS)
#define OSSL_END_ALLOW_DEPRECATED \
    #pragma error_messages(on, E_DEPRECATED_ATT, E_DEPRECATED_ATT_MESS)
#endif
#endif
#endif


#ifndef OSSL_DEPRECATED
#define OSSL_DEPRECATED(since) extern
#define OSSL_DEPRECATED_FOR(since, message) extern
#define OSSL_DEPRECATED_MESSAGE(message)
#define OSSL_BEGIN_ALLOW_DEPRECATED
#define OSSL_END_ALLOW_DEPRECATED
#endif





#ifdef OPENSSL_API_LEVEL
#error "OPENSSL_API_LEVEL must not be defined by application"
#endif


#ifdef OPENSSL_API_COMPAT
#if OPENSSL_API_COMPAT < 0x900000L
#define OPENSSL_API_LEVEL (OPENSSL_API_COMPAT)
#else
#define OPENSSL_API_LEVEL                           \
    (((OPENSSL_API_COMPAT >> 28) & 0xF) * 10000     \
        + ((OPENSSL_API_COMPAT >> 20) & 0xFF) * 100 \
        + ((OPENSSL_API_COMPAT >> 12) & 0xFF))
#endif
#endif


#ifndef OPENSSL_API_LEVEL
#if OPENSSL_CONFIGURED_API > 0
#define OPENSSL_API_LEVEL (OPENSSL_CONFIGURED_API)
#else
#define OPENSSL_API_LEVEL \
    (OPENSSL_VERSION_MAJOR * 10000 + OPENSSL_VERSION_MINOR * 100)
#endif
#endif

#if OPENSSL_API_LEVEL > OPENSSL_CONFIGURED_API
#error "The requested API level higher than the configured API compatibility level"
#endif



#if OPENSSL_API_LEVEL > (OPENSSL_VERSION_MAJOR * 10000 + OPENSSL_VERSION_MINOR * 100)
#error "OPENSSL_API_COMPAT expresses an impossible API compatibility level"
#endif

#if OPENSSL_API_LEVEL < 30000 && OPENSSL_API_LEVEL >= 20000
#error "OPENSSL_API_COMPAT expresses an impossible API compatibility level"
#endif

#if OPENSSL_API_LEVEL < 908
#error "OPENSSL_API_COMPAT expresses an impossible API compatibility level"
#endif



#undef OPENSSL_NO_DEPRECATED_4_0
#undef OPENSSL_NO_DEPRECATED_3_6
#undef OPENSSL_NO_DEPRECATED_3_5
#undef OPENSSL_NO_DEPRECATED_3_4
#undef OPENSSL_NO_DEPRECATED_3_1
#undef OPENSSL_NO_DEPRECATED_3_0
#undef OPENSSL_NO_DEPRECATED_1_1_1
#undef OPENSSL_NO_DEPRECATED_1_1_0
#undef OPENSSL_NO_DEPRECATED_1_0_2
#undef OPENSSL_NO_DEPRECATED_1_0_1
#undef OPENSSL_NO_DEPRECATED_1_0_0
#undef OPENSSL_NO_DEPRECATED_0_9_8

#if OPENSSL_API_LEVEL >= 40000
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_4_0 OSSL_DEPRECATED(4.0)
#define OSSL_DEPRECATEDIN_4_0_FOR(msg) OSSL_DEPRECATED_FOR(4.0, msg)
#else
#define OPENSSL_NO_DEPRECATED_4_0
#endif
#else
#define OSSL_DEPRECATEDIN_4_0
#define OSSL_DEPRECATEDIN_4_0_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 30600
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_3_6 OSSL_DEPRECATED(3.6)
#define OSSL_DEPRECATEDIN_3_6_FOR(msg) OSSL_DEPRECATED_FOR(3.6, msg)
#else
#define OPENSSL_NO_DEPRECATED_3_6
#endif
#else
#define OSSL_DEPRECATEDIN_3_6
#define OSSL_DEPRECATEDIN_3_6_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 30500
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_3_5 OSSL_DEPRECATED(3.5)
#define OSSL_DEPRECATEDIN_3_5_FOR(msg) OSSL_DEPRECATED_FOR(3.5, msg)
#else
#define OPENSSL_NO_DEPRECATED_3_5
#endif
#else
#define OSSL_DEPRECATEDIN_3_5
#define OSSL_DEPRECATEDIN_3_5_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 30400
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_3_4 OSSL_DEPRECATED(3.4)
#define OSSL_DEPRECATEDIN_3_4_FOR(msg) OSSL_DEPRECATED_FOR(3.4, msg)
#else
#define OPENSSL_NO_DEPRECATED_3_4
#endif
#else
#define OSSL_DEPRECATEDIN_3_4
#define OSSL_DEPRECATEDIN_3_4_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 30100
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_3_1 OSSL_DEPRECATED(3.1)
#define OSSL_DEPRECATEDIN_3_1_FOR(msg) OSSL_DEPRECATED_FOR(3.1, msg)
#else
#define OPENSSL_NO_DEPRECATED_3_1
#endif
#else
#define OSSL_DEPRECATEDIN_3_1
#define OSSL_DEPRECATEDIN_3_1_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 30000
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_3_0 OSSL_DEPRECATED(3.0)
#define OSSL_DEPRECATEDIN_3_0_FOR(msg) OSSL_DEPRECATED_FOR(3.0, msg)
#else
#define OPENSSL_NO_DEPRECATED_3_0
#endif
#else
#define OSSL_DEPRECATEDIN_3_0
#define OSSL_DEPRECATEDIN_3_0_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 10101
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_1_1_1 OSSL_DEPRECATED(1.1.1)
#define OSSL_DEPRECATEDIN_1_1_1_FOR(msg) OSSL_DEPRECATED_FOR(1.1.1, msg)
#else
#define OPENSSL_NO_DEPRECATED_1_1_1
#endif
#else
#define OSSL_DEPRECATEDIN_1_1_1
#define OSSL_DEPRECATEDIN_1_1_1_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 10100
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_1_1_0 OSSL_DEPRECATED(1.1.0)
#define OSSL_DEPRECATEDIN_1_1_0_FOR(msg) OSSL_DEPRECATED_FOR(1.1.0, msg)
#else
#define OPENSSL_NO_DEPRECATED_1_1_0
#endif
#else
#define OSSL_DEPRECATEDIN_1_1_0
#define OSSL_DEPRECATEDIN_1_1_0_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 10002
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_1_0_2 OSSL_DEPRECATED(1.0.2)
#define OSSL_DEPRECATEDIN_1_0_2_FOR(msg) OSSL_DEPRECATED_FOR(1.0.2, msg)
#else
#define OPENSSL_NO_DEPRECATED_1_0_2
#endif
#else
#define OSSL_DEPRECATEDIN_1_0_2
#define OSSL_DEPRECATEDIN_1_0_2_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 10001
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_1_0_1 OSSL_DEPRECATED(1.0.1)
#define OSSL_DEPRECATEDIN_1_0_1_FOR(msg) OSSL_DEPRECATED_FOR(1.0.1, msg)
#else
#define OPENSSL_NO_DEPRECATED_1_0_1
#endif
#else
#define OSSL_DEPRECATEDIN_1_0_1
#define OSSL_DEPRECATEDIN_1_0_1_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 10000
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_1_0_0 OSSL_DEPRECATED(1.0.0)
#define OSSL_DEPRECATEDIN_1_0_0_FOR(msg) OSSL_DEPRECATED_FOR(1.0.0, msg)
#else
#define OPENSSL_NO_DEPRECATED_1_0_0
#endif
#else
#define OSSL_DEPRECATEDIN_1_0_0
#define OSSL_DEPRECATEDIN_1_0_0_FOR(msg)
#endif
#if OPENSSL_API_LEVEL >= 908
#ifndef OPENSSL_NO_DEPRECATED
#define OSSL_DEPRECATEDIN_0_9_8 OSSL_DEPRECATED(0.9.8)
#define OSSL_DEPRECATEDIN_0_9_8_FOR(msg) OSSL_DEPRECATED_FOR(0.9.8, msg)
#else
#define OPENSSL_NO_DEPRECATED_0_9_8
#endif
#else
#define OSSL_DEPRECATEDIN_0_9_8
#define OSSL_DEPRECATEDIN_0_9_8_FOR(msg)
#endif



#ifndef OPENSSL_FILE
#ifdef OPENSSL_NO_FILENAMES
#define OPENSSL_FILE ""
#define OPENSSL_LINE 0
#else
#define OPENSSL_FILE __FILE__
#define OPENSSL_LINE __LINE__
#endif
#endif


#ifndef OPENSSL_FUNC
#if defined(__STDC_VERSION__)
#if __STDC_VERSION__ >= 199901L
#define OPENSSL_FUNC __func__
#elif defined(__GNUC__) && __GNUC__ >= 2
#define OPENSSL_FUNC __FUNCTION__
#endif
#elif defined(_MSC_VER)
#define OPENSSL_FUNC __FUNCTION__
#endif

#ifndef OPENSSL_FUNC
#define OPENSSL_FUNC "(unknown function)"
#endif
#endif

#ifndef OSSL_CRYPTO_ALLOC
#if defined(__GNUC__)
#define OSSL_CRYPTO_ALLOC __attribute__((__malloc__))
#elif defined(_MSC_VER)
#define OSSL_CRYPTO_ALLOC __declspec(restrict)
#else
#define OSSL_CRYPTO_ALLOC
#endif
#endif

#endif 

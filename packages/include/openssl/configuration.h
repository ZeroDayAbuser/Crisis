












#ifndef OPENSSL_CONFIGURATION_H
#define OPENSSL_CONFIGURATION_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OPENSSL_ALGORITHM_DEFINES
#error OPENSSL_ALGORITHM_DEFINES no longer supported
#endif






# ifndef OPENSSL_SYS_WIN64A
#  define OPENSSL_SYS_WIN64A 1
# endif
# define OPENSSL_CONFIGURED_API 40000
# ifndef OPENSSL_RAND_SEED_OS
#  define OPENSSL_RAND_SEED_OS
# endif
# ifndef OPENSSL_THREADS
#  define OPENSSL_THREADS
# endif
# ifndef OPENSSL_NO_ACVP_TESTS
#  define OPENSSL_NO_ACVP_TESTS
# endif
# ifndef OPENSSL_NO_ALLOCFAIL_TESTS
#  define OPENSSL_NO_ALLOCFAIL_TESTS
# endif
# ifndef OPENSSL_NO_ASAN
#  define OPENSSL_NO_ASAN
# endif
# ifndef OPENSSL_NO_BROTLI
#  define OPENSSL_NO_BROTLI
# endif
# ifndef OPENSSL_NO_BROTLI_DYNAMIC
#  define OPENSSL_NO_BROTLI_DYNAMIC
# endif
# ifndef OPENSSL_NO_CRYPTO_MDEBUG
#  define OPENSSL_NO_CRYPTO_MDEBUG
# endif
# ifndef OPENSSL_NO_DEMOS
#  define OPENSSL_NO_DEMOS
# endif
# ifndef OPENSSL_NO_DYNAMIC_ENGINE
#  define OPENSSL_NO_DYNAMIC_ENGINE
# endif
# ifndef OPENSSL_NO_EC_EXPLICIT_CURVES
#  define OPENSSL_NO_EC_EXPLICIT_CURVES
# endif
# ifndef OPENSSL_NO_EC_NISTP_64_GCC_128
#  define OPENSSL_NO_EC_NISTP_64_GCC_128
# endif
# ifndef OPENSSL_NO_EGD
#  define OPENSSL_NO_EGD
# endif
# ifndef OPENSSL_NO_ENGINE
#  define OPENSSL_NO_ENGINE
# endif
# ifndef OPENSSL_NO_EXTERNAL_TESTS
#  define OPENSSL_NO_EXTERNAL_TESTS
# endif
# ifndef OPENSSL_NO_FIPS_JITTER
#  define OPENSSL_NO_FIPS_JITTER
# endif
# ifndef OPENSSL_NO_FIPS_POST
#  define OPENSSL_NO_FIPS_POST
# endif
# ifndef OPENSSL_NO_FIPS_SECURITYCHECKS
#  define OPENSSL_NO_FIPS_SECURITYCHECKS
# endif
# ifndef OPENSSL_NO_FUZZ_AFL
#  define OPENSSL_NO_FUZZ_AFL
# endif
# ifndef OPENSSL_NO_FUZZ_LIBFUZZER
#  define OPENSSL_NO_FUZZ_LIBFUZZER
# endif
# ifndef OPENSSL_NO_H3DEMO
#  define OPENSSL_NO_H3DEMO
# endif
# ifndef OPENSSL_NO_HQINTEROP
#  define OPENSSL_NO_HQINTEROP
# endif
# ifndef OPENSSL_NO_JITTER
#  define OPENSSL_NO_JITTER
# endif
# ifndef OPENSSL_NO_KTLS
#  define OPENSSL_NO_KTLS
# endif
# ifndef OPENSSL_NO_LMS
#  define OPENSSL_NO_LMS
# endif
# ifndef OPENSSL_NO_MD2
#  define OPENSSL_NO_MD2
# endif
# ifndef OPENSSL_NO_MSAN
#  define OPENSSL_NO_MSAN
# endif
# ifndef OPENSSL_NO_PIE
#  define OPENSSL_NO_PIE
# endif
# ifndef OPENSSL_NO_RC5
#  define OPENSSL_NO_RC5
# endif
# ifndef OPENSSL_NO_SCTP
#  define OPENSSL_NO_SCTP
# endif
# ifndef OPENSSL_NO_SSLKEYLOG
#  define OPENSSL_NO_SSLKEYLOG
# endif
# ifndef OPENSSL_NO_STATIC_ENGINE
#  define OPENSSL_NO_STATIC_ENGINE
# endif
# ifndef OPENSSL_NO_STATIC_VCRUNTIME
#  define OPENSSL_NO_STATIC_VCRUNTIME
# endif
# ifndef OPENSSL_NO_TFO
#  define OPENSSL_NO_TFO
# endif
# ifndef OPENSSL_NO_TLS_DEPRECATED_EC
#  define OPENSSL_NO_TLS_DEPRECATED_EC
# endif
# ifndef OPENSSL_NO_TRACE
#  define OPENSSL_NO_TRACE
# endif
# ifndef OPENSSL_NO_UBSAN
#  define OPENSSL_NO_UBSAN
# endif
# ifndef OPENSSL_NO_UNIT_TEST
#  define OPENSSL_NO_UNIT_TEST
# endif
# ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
#  define OPENSSL_NO_WEAK_SSL_CIPHERS
# endif
# ifndef OPENSSL_NO_ZLIB
#  define OPENSSL_NO_ZLIB
# endif
# ifndef OPENSSL_NO_ZLIB_DYNAMIC
#  define OPENSSL_NO_ZLIB_DYNAMIC
# endif
# ifndef OPENSSL_NO_ZSTD
#  define OPENSSL_NO_ZSTD
# endif
# ifndef OPENSSL_NO_ZSTD_DYNAMIC
#  define OPENSSL_NO_ZSTD_DYNAMIC
# endif





# undef I386_ONLY








#if !defined(OPENSSL_SYS_UEFI)
    
#  undef BN_LLONG
    
    
    
#  undef SIXTY_FOUR_BIT_LONG
    
    
#  define SIXTY_FOUR_BIT
    
    
#  undef THIRTY_TWO_BIT

#endif





# define RC4_INT unsigned int


#if defined(OPENSSL_NO_COMP) || (defined(OPENSSL_NO_BROTLI) && defined(OPENSSL_NO_ZSTD) && defined(OPENSSL_NO_ZLIB))
#define OPENSSL_NO_COMP_ALG
#else
#undef OPENSSL_NO_COMP_ALG
#endif

#ifdef __cplusplus
}
#endif

#endif 

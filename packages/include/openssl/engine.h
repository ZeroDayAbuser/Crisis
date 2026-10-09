




#ifndef OPENSSL_ENGINE_H
#define OPENSSL_ENGINE_H
#pragma once

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_ENGINE_H
#endif

#include <openssl/opensslconf.h>




#ifdef OPENSSL_ENGINE_STUBS
#define ENGINE_INFO_MSG \
    " API symbol is replaced with stub to avoid linker error."

#define ENGINE_FUNC(ret_type, name, args, default_val) \
    OSSL_DEPRECATED_MESSAGE(#name ENGINE_INFO_MSG)     \
    static ossl_inline ret_type name args              \
    {                                                  \
        return default_val;           \
    }

#define ENGINE_FUNC_NOARGS(ret_type, name, default_val) \
    OSSL_DEPRECATED_MESSAGE(#name ENGINE_INFO_MSG)      \
    static ossl_inline ret_type name(void)              \
    {                                                   \
        return default_val;            \
    }

#define ENGINE_VOID_FUNC(name, args)               \
    OSSL_DEPRECATED_MESSAGE(#name ENGINE_INFO_MSG) \
    static ossl_inline void name args              \
    {                                              \
    }

#define ENGINE_VOID_FUNC_NOARGS(name)              \
    OSSL_DEPRECATED_MESSAGE(#name ENGINE_INFO_MSG) \
    static ossl_inline void name(void)             \
    {                                              \
    }
#else
#define ENGINE_INFO_MSG \
    " API symbol is removed. Define OPENSSL_ENGINE_STUBS to mask linker errors."

#define ENGINE_FUNC(ret_type, name, args, default_val) \
    OSSL_DEPRECATED_MESSAGE(#name ENGINE_INFO_MSG)     \
    ret_type name args;

#define ENGINE_FUNC_NOARGS(ret_type, name, default_val) \
    OSSL_DEPRECATED_MESSAGE(#name ENGINE_INFO_MSG)      \
    ret_type name(void);

#define ENGINE_VOID_FUNC(name, args)               \
    OSSL_DEPRECATED_MESSAGE(#name ENGINE_INFO_MSG) \
    void name args;

#define ENGINE_VOID_FUNC_NOARGS(name)              \
    OSSL_DEPRECATED_MESSAGE(#name ENGINE_INFO_MSG) \
    void name(void);
#endif

#ifdef ENGINE_FUNC

#ifndef OPENSSL_NO_DEPRECATED_1_1_0
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/rand.h>
#include <openssl/ui.h>
#include <openssl/err.h>
#endif
#include <openssl/types.h>
#include <openssl/symhacks.h>
#include <openssl/x509.h>
#include <openssl/engineerr.h>
#ifdef __cplusplus
extern "C" {
#endif


#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#endif




#define ENGINE_METHOD_RSA (unsigned int)0x0001
#define ENGINE_METHOD_DSA (unsigned int)0x0002
#define ENGINE_METHOD_DH (unsigned int)0x0004
#define ENGINE_METHOD_RAND (unsigned int)0x0008
#define ENGINE_METHOD_CIPHERS (unsigned int)0x0040
#define ENGINE_METHOD_DIGESTS (unsigned int)0x0080
#define ENGINE_METHOD_PKEY_METHS (unsigned int)0x0200
#define ENGINE_METHOD_PKEY_ASN1_METHS (unsigned int)0x0400
#define ENGINE_METHOD_EC (unsigned int)0x0800

#define ENGINE_METHOD_ALL (unsigned int)0xFFFF
#define ENGINE_METHOD_NONE (unsigned int)0x0000




#define ENGINE_TABLE_FLAG_NOINIT (unsigned int)0x0001




#define ENGINE_FLAGS_MANUAL_CMD_CTRL (int)0x0002




#define ENGINE_FLAGS_BY_ID_COPY (int)0x0004




#define ENGINE_FLAGS_NO_REGISTER_ALL (int)0x0008




#define ENGINE_CMD_FLAG_NUMERIC (unsigned int)0x0001




#define ENGINE_CMD_FLAG_STRING (unsigned int)0x0002




#define ENGINE_CMD_FLAG_NO_INPUT (unsigned int)0x0004




#define ENGINE_CMD_FLAG_INTERNAL (unsigned int)0x0008




#define ENGINE_CTRL_SET_LOGSTREAM 1
#define ENGINE_CTRL_SET_PASSWORD_CALLBACK 2
#define ENGINE_CTRL_HUP 3


#define ENGINE_CTRL_SET_USER_INTERFACE 4
#define ENGINE_CTRL_SET_CALLBACK_DATA 5



#define ENGINE_CTRL_LOAD_CONFIGURATION 6



#define ENGINE_CTRL_LOAD_SECTION 7




#define ENGINE_CTRL_HAS_CTRL_FUNCTION 10




#define ENGINE_CTRL_GET_FIRST_CMD_TYPE 11




#define ENGINE_CTRL_GET_NEXT_CMD_TYPE 12




#define ENGINE_CTRL_GET_CMD_FROM_NAME 13




#define ENGINE_CTRL_GET_NAME_LEN_FROM_CMD 14
#define ENGINE_CTRL_GET_NAME_FROM_CMD 15

#define ENGINE_CTRL_GET_DESC_LEN_FROM_CMD 16
#define ENGINE_CTRL_GET_DESC_FROM_CMD 17




#define ENGINE_CTRL_GET_CMD_FLAGS 18




#define ENGINE_CMD_BASE 200




#define ENGINE_CTRL_CHIL_SET_FORKCHECK 100




#define ENGINE_CTRL_CHIL_NO_LOCKING 101




typedef struct ENGINE_CMD_DEFN_st {
    unsigned int cmd_num;
    const char *cmd_name;
    const char *cmd_desc;
    unsigned int cmd_flags;
} ENGINE_CMD_DEFN;


typedef int (*ENGINE_GEN_FUNC_PTR)(void);

typedef int (*ENGINE_GEN_INT_FUNC_PTR)(ENGINE *);

typedef int (*ENGINE_CTRL_FUNC_PTR)(ENGINE *, int, long, void *,
    void (*f)(void));

typedef EVP_PKEY *(*ENGINE_LOAD_KEY_PTR)(ENGINE *, const char *,
    UI_METHOD *ui_method,
    void *callback_data);
typedef int (*ENGINE_SSL_CLIENT_CERT_PTR)(ENGINE *, SSL *ssl,
    STACK_OF(X509_NAME) *ca_dn,
    X509 **pcert, EVP_PKEY **pkey,
    STACK_OF(X509) **pother,
    UI_METHOD *ui_method,
    void *callback_data);




typedef int (*ENGINE_CIPHERS_PTR)(ENGINE *, const EVP_CIPHER **,
    const int **, int);
typedef int (*ENGINE_DIGESTS_PTR)(ENGINE *, const EVP_MD **, const int **,
    int);
typedef int (*ENGINE_PKEY_METHS_PTR)(ENGINE *, EVP_PKEY_METHOD **,
    const int **, int);
typedef int (*ENGINE_PKEY_ASN1_METHS_PTR)(ENGINE *, EVP_PKEY_ASN1_METHOD **,
    const int **, int);




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC_NOARGS(ENGINE *, ENGINE_get_first, NULL)


ENGINE_FUNC_NOARGS(ENGINE *, ENGINE_get_last, NULL)

#endif

#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(ENGINE *, ENGINE_get_next, (ENGINE *e), NULL)


ENGINE_FUNC(ENGINE *, ENGINE_get_prev, (ENGINE *e), NULL)

#endif

#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_add, (ENGINE *e), 0)
#endif

#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_remove, (ENGINE *e), 0)
#endif

#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(ENGINE *, ENGINE_by_id, (const char *id), NULL)
#endif

#ifndef OPENSSL_NO_DEPRECATED_1_1_0
ENGINE_FUNC_NOARGS(int, ENGINE_load_openssl, 0)
ENGINE_FUNC_NOARGS(int, ENGINE_load_dynamic, 0)
ENGINE_FUNC_NOARGS(int, ENGINE_load_cryptodev, 0)
ENGINE_FUNC_NOARGS(int, ENGINE_load_rdrand, 0)
#ifndef OPENSSL_NO_STATIC_ENGINE
ENGINE_FUNC_NOARGS(int, ENGINE_load_padlock, 0)
ENGINE_FUNC_NOARGS(int, ENGINE_load_capi, 0)
ENGINE_FUNC_NOARGS(int, ENGINE_load_afalg, 0)
#endif
#endif
#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_VOID_FUNC_NOARGS(ENGINE_load_builtin_engines)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC_NOARGS(unsigned int, ENGINE_get_table_flags, 0)


ENGINE_VOID_FUNC(ENGINE_set_table_flags, (unsigned int flags))
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_register_RSA, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_RSA, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_RSA)

ENGINE_FUNC(int, ENGINE_register_DSA, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_DSA, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_DSA)

ENGINE_FUNC(int, ENGINE_register_EC, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_EC, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_EC)

ENGINE_FUNC(int, ENGINE_register_DH, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_DH, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_DH)

ENGINE_FUNC(int, ENGINE_register_RAND, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_RAND, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_RAND)

ENGINE_FUNC(int, ENGINE_register_ciphers, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_ciphers, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_ciphers)

ENGINE_FUNC(int, ENGINE_register_digests, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_digests, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_digests)

ENGINE_FUNC(int, ENGINE_register_pkey_meths, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_pkey_meths, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_pkey_meths)

ENGINE_FUNC(int, ENGINE_register_pkey_asn1_meths, (ENGINE *e), 0)

ENGINE_VOID_FUNC(ENGINE_unregister_pkey_asn1_meths, (ENGINE *e))

ENGINE_VOID_FUNC_NOARGS(ENGINE_register_all_pkey_asn1_meths)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_register_complete, (ENGINE *e), 0)

ENGINE_FUNC_NOARGS(int, ENGINE_register_all_complete, 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0



ENGINE_FUNC(int, ENGINE_ctrl, (ENGINE *e, int cmd, long i, void *p, void (*f)(void)), 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_cmd_is_executable, (ENGINE *e, int cmd), 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0




ENGINE_FUNC(int, ENGINE_ctrl_cmd, (ENGINE *e, const char *cmd_name, long i, void *p, void (*f)(void), int cmd_optional), 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0




ENGINE_FUNC(int, ENGINE_ctrl_cmd_string,
    (ENGINE *e, const char *cmd_name, const char *arg, int cmd_optional), 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC_NOARGS(ENGINE *, ENGINE_new, NULL)

ENGINE_FUNC(int, ENGINE_free, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_up_ref, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_set_id, (ENGINE *e, const char *id), 0)

ENGINE_FUNC(int, ENGINE_set_name, (ENGINE *e, const char *name), 0)



ENGINE_FUNC(int, ENGINE_set_RSA, (ENGINE *e, const RSA_METHOD *rsa_meth), 0)



ENGINE_FUNC(int, ENGINE_set_DSA, (ENGINE *e, const DSA_METHOD *dsa_meth), 0)



ENGINE_FUNC(int, ENGINE_set_EC, (ENGINE *e, const EC_KEY_METHOD *ecdsa_meth), 0)



ENGINE_FUNC(int, ENGINE_set_DH, (ENGINE *e, const DH_METHOD *dh_meth), 0)



ENGINE_FUNC(int, ENGINE_set_RAND, (ENGINE *e, const RAND_METHOD *rand_meth), 0)



ENGINE_FUNC(int, ENGINE_set_destroy_function,
    (ENGINE *e, ENGINE_GEN_INT_FUNC_PTR destroy_f), 0)



ENGINE_FUNC(int, ENGINE_set_init_function,
    (ENGINE *e, ENGINE_GEN_INT_FUNC_PTR init_f), 0)



ENGINE_FUNC(int, ENGINE_set_finish_function,
    (ENGINE *e, ENGINE_GEN_INT_FUNC_PTR finish_f), 0)



ENGINE_FUNC(int, ENGINE_set_ctrl_function,
    (ENGINE *e, ENGINE_CTRL_FUNC_PTR ctrl_f), 0)



ENGINE_FUNC(int, ENGINE_set_load_privkey_function,
    (ENGINE *e, ENGINE_LOAD_KEY_PTR loadpriv_f), 0)



ENGINE_FUNC(int, ENGINE_set_load_pubkey_function,
    (ENGINE *e, ENGINE_LOAD_KEY_PTR loadpub_f), 0)




ENGINE_FUNC(int, ENGINE_set_load_ssl_client_cert_function,
    (ENGINE *e, ENGINE_SSL_CLIENT_CERT_PTR loadssl_f), 0)



ENGINE_FUNC(int, ENGINE_set_ciphers, (ENGINE *e, ENGINE_CIPHERS_PTR f), 0)



ENGINE_FUNC(int, ENGINE_set_digests, (ENGINE *e, ENGINE_DIGESTS_PTR f), 0)



ENGINE_FUNC(int, ENGINE_set_pkey_meths, (ENGINE *e, ENGINE_PKEY_METHS_PTR f), 0)



ENGINE_FUNC(int, ENGINE_set_pkey_asn1_meths, (ENGINE *e, ENGINE_PKEY_ASN1_METHS_PTR f),
    0)


ENGINE_FUNC(int, ENGINE_set_flags, (ENGINE *e, int flags), 0)



ENGINE_FUNC(int, ENGINE_set_cmd_defns, (ENGINE *e, const ENGINE_CMD_DEFN *defns), 0)
#endif

#ifndef OPENSSL_ENGINE_STUBS
#define ENGINE_get_ex_new_index(l, p, newf, dupf, freef) \
    CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_ENGINE, l, p, newf, dupf, freef)
#else
ENGINE_FUNC(int, ENGINE_get_ex_new_index, (long l, void *p, CRYPTO_EX_new *newf, CRYPTO_EX_dup *dupf, CRYPTO_EX_free *freef), 0)
#endif
#ifndef OPENSSL_NO_DEPRECATED_3_0



ENGINE_FUNC(int, ENGINE_set_ex_data, (ENGINE *e, int idx, void *arg), 0)

ENGINE_FUNC(void *, ENGINE_get_ex_data, (ENGINE *e, int idx), NULL)
#endif

#ifndef OPENSSL_NO_DEPRECATED_1_1_0




#define ENGINE_cleanup() \
    while (0)            \
    continue
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(const char *, ENGINE_get_id, (const ENGINE *e), NULL)

ENGINE_FUNC(const char *, ENGINE_get_name, (const ENGINE *e), NULL)

ENGINE_FUNC(const RSA_METHOD *, ENGINE_get_RSA, (const ENGINE *e), NULL)

ENGINE_FUNC(const DSA_METHOD *, ENGINE_get_DSA, (const ENGINE *e), NULL)

ENGINE_FUNC(const EC_KEY_METHOD *, ENGINE_get_EC, (const ENGINE *e), NULL)

ENGINE_FUNC(const DH_METHOD *, ENGINE_get_DH, (const ENGINE *e), NULL)

ENGINE_FUNC(const RAND_METHOD *, ENGINE_get_RAND, (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_GEN_INT_FUNC_PTR, ENGINE_get_destroy_function,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_GEN_INT_FUNC_PTR, ENGINE_get_init_function,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_GEN_INT_FUNC_PTR, ENGINE_get_finish_function,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_CTRL_FUNC_PTR, ENGINE_get_ctrl_function,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_LOAD_KEY_PTR, ENGINE_get_load_privkey_function,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_LOAD_KEY_PTR, ENGINE_get_load_pubkey_function,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_SSL_CLIENT_CERT_PTR, ENGINE_get_ssl_client_cert_function,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_CIPHERS_PTR, ENGINE_get_ciphers,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_DIGESTS_PTR, ENGINE_get_digests,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_PKEY_METHS_PTR, ENGINE_get_pkey_meths,
    (const ENGINE *e), NULL)



ENGINE_FUNC(ENGINE_PKEY_ASN1_METHS_PTR, ENGINE_get_pkey_asn1_meths,
    (const ENGINE *e), NULL)



ENGINE_FUNC(const EVP_CIPHER *, ENGINE_get_cipher,
    (ENGINE *e, int nid), NULL)



ENGINE_FUNC(const EVP_MD *, ENGINE_get_digest,
    (ENGINE *e, int nid), NULL)



ENGINE_FUNC(const EVP_PKEY_METHOD *, ENGINE_get_pkey_meth,
    (ENGINE *e, int nid), NULL)



ENGINE_FUNC(const EVP_PKEY_ASN1_METHOD *, ENGINE_get_pkey_asn1_meth,
    (ENGINE *e, int nid), NULL)




ENGINE_FUNC(const EVP_PKEY_ASN1_METHOD *, ENGINE_get_pkey_asn1_meth_str,
    (ENGINE *e, const char *str, int len), NULL)




ENGINE_FUNC(const EVP_PKEY_ASN1_METHOD *, ENGINE_pkey_asn1_find_str,
    (ENGINE **pe, const char *str, int len), NULL)



ENGINE_FUNC(const ENGINE_CMD_DEFN *, ENGINE_get_cmd_defns,
    (const ENGINE *e), NULL)

ENGINE_FUNC(int, ENGINE_get_flags, (const ENGINE *e), 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_init, (ENGINE *e), 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_finish, (ENGINE *e), 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0




ENGINE_FUNC(EVP_PKEY *, ENGINE_load_private_key,
    (ENGINE *e, const char *key_id, UI_METHOD *ui_method,
        void *callback_data),
    NULL)




ENGINE_FUNC(EVP_PKEY *, ENGINE_load_public_key,
    (ENGINE *e, const char *key_id, UI_METHOD *ui_method,
        void *callback_data),
    NULL)




ENGINE_FUNC(int, ENGINE_load_ssl_client_cert,
    (ENGINE *e, SSL *s, STACK_OF(X509_NAME) *ca_dn, X509 **pcert,
        EVP_PKEY **ppkey, STACK_OF(X509) **pother, UI_METHOD *ui_method,
        void *callback_data),
    0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC_NOARGS(ENGINE *, ENGINE_get_default_RSA, NULL)
#endif

#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC_NOARGS(ENGINE *, ENGINE_get_default_DSA, NULL)

ENGINE_FUNC_NOARGS(ENGINE *, ENGINE_get_default_EC, NULL)

ENGINE_FUNC_NOARGS(ENGINE *, ENGINE_get_default_DH, NULL)

ENGINE_FUNC_NOARGS(ENGINE *, ENGINE_get_default_RAND, NULL)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(ENGINE *, ENGINE_get_cipher_engine, (int nid), NULL)

ENGINE_FUNC(ENGINE *, ENGINE_get_digest_engine, (int nid), NULL)

ENGINE_FUNC(ENGINE *, ENGINE_get_pkey_meth_engine, (int nid), NULL)

ENGINE_FUNC(ENGINE *, ENGINE_get_pkey_asn1_meth_engine, (int nid), NULL)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_set_default_RSA, (ENGINE *e), 0)



ENGINE_FUNC(int, ENGINE_set_default_string, (ENGINE *e, const char *def_list), 0)
#endif

#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, ENGINE_set_default_DSA, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_set_default_EC, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_set_default_DH, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_set_default_RAND, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_set_default_ciphers, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_set_default_digests, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_set_default_pkey_meths, (ENGINE *e), 0)

ENGINE_FUNC(int, ENGINE_set_default_pkey_asn1_meths, (ENGINE *e), 0)
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0



ENGINE_FUNC(int, ENGINE_set_default, (ENGINE *e, unsigned int flags), 0)

ENGINE_VOID_FUNC_NOARGS(ENGINE_add_conf_module)
#endif




#define OSSL_DYNAMIC_VERSION (unsigned long)0x00030000




#define OSSL_DYNAMIC_OLDEST (unsigned long)0x00030000




typedef void *(*dyn_MEM_malloc_fn)(size_t, const char *, int);
typedef void *(*dyn_MEM_realloc_fn)(void *, size_t, const char *, int);
typedef void (*dyn_MEM_free_fn)(void *, const char *, int);
typedef struct st_dynamic_MEM_fns {
    dyn_MEM_malloc_fn malloc_fn;
    dyn_MEM_realloc_fn realloc_fn;
    dyn_MEM_free_fn free_fn;
} dynamic_MEM_fns;




typedef struct st_dynamic_fns {
    void *static_state;
    dynamic_MEM_fns mem_fns;
} dynamic_fns;




typedef unsigned long (*dynamic_v_check_fn)(unsigned long ossl_version);
#define IMPLEMENT_DYNAMIC_CHECK_FN()                       \
    OPENSSL_EXPORT unsigned long v_check(unsigned long v); \
    OPENSSL_EXPORT unsigned long v_check(unsigned long v)  \
    {                                                      \
        return 0;                                          \
    }




typedef int (*dynamic_bind_engine)(ENGINE *e, const char *id,
    const dynamic_fns *fns);
#define IMPLEMENT_DYNAMIC_BIND_FN(fn)                                   \
    OPENSSL_EXPORT                                                      \
    int bind_engine(ENGINE *e, const char *id, const dynamic_fns *fns); \
    OPENSSL_EXPORT                                                      \
    int bind_engine(ENGINE *e, const char *id, const dynamic_fns *fns)  \
    {                                                                   \
        return 0;                                                       \
    }




ENGINE_FUNC_NOARGS(void *, ENGINE_get_static_state, NULL)

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
#ifndef OPENSSL_NO_DEPRECATED_1_1_0

ENGINE_VOID_FUNC_NOARGS(ENGINE_setup_bsd_cryptodev)
#endif
#endif




#ifndef OPENSSL_NO_DEPRECATED_3_0

ENGINE_FUNC(int, EVP_PKEY_set1_engine, (EVP_PKEY *pkey, ENGINE *e), 0)


ENGINE_FUNC(ENGINE *, EVP_PKEY_get0_engine, (const EVP_PKEY *pkey), NULL)


ENGINE_FUNC(ENGINE *, DH_get0_engine, (DH *d), NULL)


ENGINE_FUNC(ENGINE *, RSA_get0_engine, (const RSA *r), NULL)


ENGINE_FUNC(ENGINE *, DSA_get0_engine, (DSA *d), NULL)


ENGINE_FUNC(ENGINE *, EC_KEY_get0_engine, (const EC_KEY *eckey), NULL)


ENGINE_FUNC(const ENGINE *, OSSL_STORE_LOADER_get0_engine, (const OSSL_STORE_LOADER *loader),
    NULL)


ENGINE_FUNC(int, RAND_set_rand_engine, (ENGINE *engine), 0)


ENGINE_FUNC_NOARGS(int, ERR_load_ENGINE_strings, 1)
#endif




ENGINE_FUNC(int, TS_CONF_set_crypto_device,
    (CONF *conf, const char *section, const char *device), 0)


ENGINE_FUNC(int, TS_CONF_set_default_engine, (const char *name), 0)


ENGINE_FUNC(int, SSL_CTX_set_client_cert_engine, (SSL_CTX *ctx, ENGINE *e), 0)

#ifdef __cplusplus
}
#endif

#undef ENGINE_INFO_MSG
#undef ENGINE_FUNC
#undef ENGINE_VOID_FUNC
#undef ENGINE_FUNC_NOARGS
#undef ENGINE_VOID_FUNC_NOARGS

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
#endif

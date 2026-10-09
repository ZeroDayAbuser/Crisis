




#ifndef OPENSSL_ERR_H
#define OPENSSL_ERR_H
#pragma once

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_ERR_H
#endif

#include <openssl/e_os2.h>

#ifndef OPENSSL_NO_STDIO
#include <stdio.h>
#include <stdlib.h>
#endif

#include <openssl/types.h>
#include <openssl/bio.h>
#include <openssl/lhash.h>
#include <openssl/cryptoerr_legacy.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPENSSL_NO_DEPRECATED_3_0
#ifndef OPENSSL_NO_FILENAMES
#define ERR_PUT_error(l, f, r, fn, ln) ERR_put_error(l, f, r, fn, ln)
#else
#define ERR_PUT_error(l, f, r, fn, ln) ERR_put_error(l, f, r, NULL, 0)
#endif
#endif

#include <limits.h>
#include <errno.h>

#define ERR_TXT_MALLOCED 0x01
#define ERR_TXT_STRING 0x02


#define ERR_LIB_NONE 1
#define ERR_LIB_SYS 2
#define ERR_LIB_BN 3
#define ERR_LIB_RSA 4
#define ERR_LIB_DH 5
#define ERR_LIB_EVP 6
#define ERR_LIB_BUF 7
#define ERR_LIB_OBJ 8
#define ERR_LIB_PEM 9
#define ERR_LIB_DSA 10
#define ERR_LIB_X509 11

#define ERR_LIB_ASN1 13
#define ERR_LIB_CONF 14
#define ERR_LIB_CRYPTO 15
#define ERR_LIB_EC 16
#define ERR_LIB_SSL 20




#define ERR_LIB_BIO 32
#define ERR_LIB_PKCS7 33
#define ERR_LIB_X509V3 34
#define ERR_LIB_PKCS12 35
#define ERR_LIB_RAND 36
#define ERR_LIB_DSO 37
#define ERR_LIB_ENGINE 38
#define ERR_LIB_OCSP 39
#define ERR_LIB_UI 40
#define ERR_LIB_COMP 41
#define ERR_LIB_ECDSA 42
#define ERR_LIB_ECDH 43
#define ERR_LIB_OSSL_STORE 44
#define ERR_LIB_FIPS 45
#define ERR_LIB_CMS 46
#define ERR_LIB_TS 47
#define ERR_LIB_HMAC 48

#define ERR_LIB_CT 50
#define ERR_LIB_ASYNC 51
#define ERR_LIB_KDF 52
#define ERR_LIB_SM2 53
#define ERR_LIB_ESS 54
#define ERR_LIB_PROP 55
#define ERR_LIB_CRMF 56
#define ERR_LIB_PROV 57
#define ERR_LIB_CMP 58
#define ERR_LIB_OSSL_ENCODER 59
#define ERR_LIB_OSSL_DECODER 60
#define ERR_LIB_HTTP 61

#define ERR_LIB_USER 128

#ifndef OPENSSL_NO_DEPRECATED_3_0
#define ASN1err(f, r) ERR_raise_data(ERR_LIB_ASN1, (r), NULL)
#define ASYNCerr(f, r) ERR_raise_data(ERR_LIB_ASYNC, (r), NULL)
#define BIOerr(f, r) ERR_raise_data(ERR_LIB_BIO, (r), NULL)
#define BNerr(f, r) ERR_raise_data(ERR_LIB_BN, (r), NULL)
#define BUFerr(f, r) ERR_raise_data(ERR_LIB_BUF, (r), NULL)
#define CMPerr(f, r) ERR_raise_data(ERR_LIB_CMP, (r), NULL)
#define CMSerr(f, r) ERR_raise_data(ERR_LIB_CMS, (r), NULL)
#define COMPerr(f, r) ERR_raise_data(ERR_LIB_COMP, (r), NULL)
#define CONFerr(f, r) ERR_raise_data(ERR_LIB_CONF, (r), NULL)
#define CRMFerr(f, r) ERR_raise_data(ERR_LIB_CRMF, (r), NULL)
#define CRYPTOerr(f, r) ERR_raise_data(ERR_LIB_CRYPTO, (r), NULL)
#define CTerr(f, r) ERR_raise_data(ERR_LIB_CT, (r), NULL)
#define DHerr(f, r) ERR_raise_data(ERR_LIB_DH, (r), NULL)
#define DSAerr(f, r) ERR_raise_data(ERR_LIB_DSA, (r), NULL)
#define DSOerr(f, r) ERR_raise_data(ERR_LIB_DSO, (r), NULL)
#define ECDHerr(f, r) ERR_raise_data(ERR_LIB_ECDH, (r), NULL)
#define ECDSAerr(f, r) ERR_raise_data(ERR_LIB_ECDSA, (r), NULL)
#define ECerr(f, r) ERR_raise_data(ERR_LIB_EC, (r), NULL)
#define ENGINEerr(f, r) ERR_raise_data(ERR_LIB_ENGINE, (r), NULL)
#define ESSerr(f, r) ERR_raise_data(ERR_LIB_ESS, (r), NULL)
#define EVPerr(f, r) ERR_raise_data(ERR_LIB_EVP, (r), NULL)
#define FIPSerr(f, r) ERR_raise_data(ERR_LIB_FIPS, (r), NULL)
#define HMACerr(f, r) ERR_raise_data(ERR_LIB_HMAC, (r), NULL)
#define HTTPerr(f, r) ERR_raise_data(ERR_LIB_HTTP, (r), NULL)
#define KDFerr(f, r) ERR_raise_data(ERR_LIB_KDF, (r), NULL)
#define OBJerr(f, r) ERR_raise_data(ERR_LIB_OBJ, (r), NULL)
#define OCSPerr(f, r) ERR_raise_data(ERR_LIB_OCSP, (r), NULL)
#define OSSL_STOREerr(f, r) ERR_raise_data(ERR_LIB_OSSL_STORE, (r), NULL)
#define PEMerr(f, r) ERR_raise_data(ERR_LIB_PEM, (r), NULL)
#define PKCS12err(f, r) ERR_raise_data(ERR_LIB_PKCS12, (r), NULL)
#define PKCS7err(f, r) ERR_raise_data(ERR_LIB_PKCS7, (r), NULL)
#define PROPerr(f, r) ERR_raise_data(ERR_LIB_PROP, (r), NULL)
#define PROVerr(f, r) ERR_raise_data(ERR_LIB_PROV, (r), NULL)
#define RANDerr(f, r) ERR_raise_data(ERR_LIB_RAND, (r), NULL)
#define RSAerr(f, r) ERR_raise_data(ERR_LIB_RSA, (r), NULL)
#define KDFerr(f, r) ERR_raise_data(ERR_LIB_KDF, (r), NULL)
#define SM2err(f, r) ERR_raise_data(ERR_LIB_SM2, (r), NULL)
#define SSLerr(f, r) ERR_raise_data(ERR_LIB_SSL, (r), NULL)
#define SYSerr(f, r) ERR_raise_data(ERR_LIB_SYS, (r), NULL)
#define TSerr(f, r) ERR_raise_data(ERR_LIB_TS, (r), NULL)
#define UIerr(f, r) ERR_raise_data(ERR_LIB_UI, (r), NULL)
#define X509V3err(f, r) ERR_raise_data(ERR_LIB_X509V3, (r), NULL)
#define X509err(f, r) ERR_raise_data(ERR_LIB_X509, (r), NULL)
#endif




#define ERR_SYSTEM_FLAG ((unsigned int)INT_MAX + 1)
#define ERR_SYSTEM_MASK ((unsigned int)INT_MAX)




#define ERR_LIB_OFFSET 23L
#define ERR_LIB_MASK 0xFF
#define ERR_RFLAGS_OFFSET 18L
#define ERR_RFLAGS_MASK 0x1F
#define ERR_REASON_MASK 0X7FFFFF




#define ERR_RFLAG_FATAL (0x1 << ERR_RFLAGS_OFFSET)
#define ERR_RFLAG_COMMON (0x2 << ERR_RFLAGS_OFFSET)

#define ERR_SYSTEM_ERROR(errcode) (((errcode) & ERR_SYSTEM_FLAG) != 0)

static ossl_unused ossl_inline int ERR_GET_LIB(unsigned long errcode)
{
    if (ERR_SYSTEM_ERROR(errcode))
        return ERR_LIB_SYS;
    return (errcode >> ERR_LIB_OFFSET) & ERR_LIB_MASK;
}

static ossl_unused ossl_inline int ERR_GET_RFLAGS(unsigned long errcode)
{
    if (ERR_SYSTEM_ERROR(errcode))
        return 0;
    return errcode & (ERR_RFLAGS_MASK << ERR_RFLAGS_OFFSET);
}

static ossl_unused ossl_inline int ERR_GET_REASON(unsigned long errcode)
{
    if (ERR_SYSTEM_ERROR(errcode))
        return errcode & ERR_SYSTEM_MASK;
    return errcode & ERR_REASON_MASK;
}

static ossl_unused ossl_inline int ERR_FATAL_ERROR(unsigned long errcode)
{
    return (ERR_GET_RFLAGS(errcode) & ERR_RFLAG_FATAL) != 0;
}

static ossl_unused ossl_inline int ERR_COMMON_ERROR(unsigned long errcode)
{
    return (ERR_GET_RFLAGS(errcode) & ERR_RFLAG_COMMON) != 0;
}




#define ERR_PACK(lib, func, reason) \
    ((((unsigned long)(lib) & ERR_LIB_MASK) << ERR_LIB_OFFSET) | (((unsigned long)(reason) & ERR_REASON_MASK)))




#define ERR_R_SYS_LIB (ERR_LIB_SYS  | ERR_RFLAG_COMMON)
#define ERR_R_BN_LIB (ERR_LIB_BN  | ERR_RFLAG_COMMON)
#define ERR_R_RSA_LIB (ERR_LIB_RSA  | ERR_RFLAG_COMMON)
#define ERR_R_DH_LIB (ERR_LIB_DH  | ERR_RFLAG_COMMON)
#define ERR_R_EVP_LIB (ERR_LIB_EVP  | ERR_RFLAG_COMMON)
#define ERR_R_BUF_LIB (ERR_LIB_BUF  | ERR_RFLAG_COMMON)
#define ERR_R_OBJ_LIB (ERR_LIB_OBJ  | ERR_RFLAG_COMMON)
#define ERR_R_PEM_LIB (ERR_LIB_PEM  | ERR_RFLAG_COMMON)
#define ERR_R_DSA_LIB (ERR_LIB_DSA  | ERR_RFLAG_COMMON)
#define ERR_R_X509_LIB (ERR_LIB_X509  | ERR_RFLAG_COMMON)
#define ERR_R_ASN1_LIB (ERR_LIB_ASN1  | ERR_RFLAG_COMMON)
#define ERR_R_CONF_LIB (ERR_LIB_CONF  | ERR_RFLAG_COMMON)
#define ERR_R_CRYPTO_LIB (ERR_LIB_CRYPTO  | ERR_RFLAG_COMMON)
#define ERR_R_EC_LIB (ERR_LIB_EC  | ERR_RFLAG_COMMON)
#define ERR_R_SSL_LIB (ERR_LIB_SSL  | ERR_RFLAG_COMMON)
#define ERR_R_BIO_LIB (ERR_LIB_BIO  | ERR_RFLAG_COMMON)
#define ERR_R_PKCS7_LIB (ERR_LIB_PKCS7  | ERR_RFLAG_COMMON)
#define ERR_R_X509V3_LIB (ERR_LIB_X509V3  | ERR_RFLAG_COMMON)
#define ERR_R_PKCS12_LIB (ERR_LIB_PKCS12  | ERR_RFLAG_COMMON)
#define ERR_R_RAND_LIB (ERR_LIB_RAND  | ERR_RFLAG_COMMON)
#define ERR_R_DSO_LIB (ERR_LIB_DSO  | ERR_RFLAG_COMMON)
#define ERR_R_ENGINE_LIB (ERR_LIB_ENGINE  | ERR_RFLAG_COMMON)
#define ERR_R_UI_LIB (ERR_LIB_UI  | ERR_RFLAG_COMMON)
#define ERR_R_ECDSA_LIB (ERR_LIB_ECDSA  | ERR_RFLAG_COMMON)
#define ERR_R_OSSL_STORE_LIB (ERR_LIB_OSSL_STORE  | ERR_RFLAG_COMMON)
#define ERR_R_CMS_LIB (ERR_LIB_CMS  | ERR_RFLAG_COMMON)
#define ERR_R_TS_LIB (ERR_LIB_TS  | ERR_RFLAG_COMMON)
#define ERR_R_CT_LIB (ERR_LIB_CT  | ERR_RFLAG_COMMON)
#define ERR_R_PROV_LIB (ERR_LIB_PROV  | ERR_RFLAG_COMMON)
#define ERR_R_ESS_LIB (ERR_LIB_ESS  | ERR_RFLAG_COMMON)
#define ERR_R_CMP_LIB (ERR_LIB_CMP  | ERR_RFLAG_COMMON)
#define ERR_R_OSSL_ENCODER_LIB (ERR_LIB_OSSL_ENCODER  | ERR_RFLAG_COMMON)
#define ERR_R_OSSL_DECODER_LIB (ERR_LIB_OSSL_DECODER  | ERR_RFLAG_COMMON)


#define ERR_R_FATAL (ERR_RFLAG_FATAL | ERR_RFLAG_COMMON)
#define ERR_R_MALLOC_FAILURE (256 | ERR_R_FATAL)
#define ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED (257 | ERR_R_FATAL)
#define ERR_R_PASSED_NULL_PARAMETER (258 | ERR_R_FATAL)
#define ERR_R_INTERNAL_ERROR (259 | ERR_R_FATAL)
#define ERR_R_DISABLED (260 | ERR_R_FATAL)
#define ERR_R_INIT_FAIL (261 | ERR_R_FATAL)
#define ERR_R_PASSED_INVALID_ARGUMENT (262 | ERR_RFLAG_COMMON)
#define ERR_R_OPERATION_FAIL (263 | ERR_R_FATAL)
#define ERR_R_INVALID_PROVIDER_FUNCTIONS (264 | ERR_R_FATAL)
#define ERR_R_INTERRUPTED_OR_CANCELLED (265 | ERR_RFLAG_COMMON)
#define ERR_R_NESTED_ASN1_ERROR (266 | ERR_RFLAG_COMMON)
#define ERR_R_MISSING_ASN1_EOS (267 | ERR_RFLAG_COMMON)
#define ERR_R_UNSUPPORTED (268 | ERR_RFLAG_COMMON)
#define ERR_R_FETCH_FAILED (269 | ERR_RFLAG_COMMON)
#define ERR_R_INVALID_PROPERTY_DEFINITION (270 | ERR_RFLAG_COMMON)
#define ERR_R_UNABLE_TO_GET_READ_LOCK (271 | ERR_R_FATAL)
#define ERR_R_UNABLE_TO_GET_WRITE_LOCK (272 | ERR_R_FATAL)

typedef struct ERR_string_data_st {
    unsigned long error;
    const char *string;
} ERR_STRING_DATA;


DEFINE_LHASH_OF_INTERNAL(ERR_STRING_DATA);
#define lh_ERR_STRING_DATA_new(hfn, cmp) ((LHASH_OF(ERR_STRING_DATA) *)OPENSSL_LH_set_thunks(OPENSSL_LH_new(ossl_check_ERR_STRING_DATA_lh_hashfunc_type(hfn), ossl_check_ERR_STRING_DATA_lh_compfunc_type(cmp)), lh_ERR_STRING_DATA_hash_thunk, lh_ERR_STRING_DATA_comp_thunk, lh_ERR_STRING_DATA_doall_thunk, lh_ERR_STRING_DATA_doall_arg_thunk))
#define lh_ERR_STRING_DATA_free(lh) OPENSSL_LH_free(ossl_check_ERR_STRING_DATA_lh_type(lh))
#define lh_ERR_STRING_DATA_flush(lh) OPENSSL_LH_flush(ossl_check_ERR_STRING_DATA_lh_type(lh))
#define lh_ERR_STRING_DATA_insert(lh, ptr) ((ERR_STRING_DATA *)OPENSSL_LH_insert(ossl_check_ERR_STRING_DATA_lh_type(lh), ossl_check_ERR_STRING_DATA_lh_plain_type(ptr)))
#define lh_ERR_STRING_DATA_delete(lh, ptr) ((ERR_STRING_DATA *)OPENSSL_LH_delete(ossl_check_ERR_STRING_DATA_lh_type(lh), ossl_check_const_ERR_STRING_DATA_lh_plain_type(ptr)))
#define lh_ERR_STRING_DATA_retrieve(lh, ptr) ((ERR_STRING_DATA *)OPENSSL_LH_retrieve(ossl_check_ERR_STRING_DATA_lh_type(lh), ossl_check_const_ERR_STRING_DATA_lh_plain_type(ptr)))
#define lh_ERR_STRING_DATA_error(lh) OPENSSL_LH_error(ossl_check_ERR_STRING_DATA_lh_type(lh))
#define lh_ERR_STRING_DATA_num_items(lh) OPENSSL_LH_num_items(ossl_check_ERR_STRING_DATA_lh_type(lh))
#define lh_ERR_STRING_DATA_node_stats_bio(lh, out) OPENSSL_LH_node_stats_bio(ossl_check_const_ERR_STRING_DATA_lh_type(lh), out)
#define lh_ERR_STRING_DATA_node_usage_stats_bio(lh, out) OPENSSL_LH_node_usage_stats_bio(ossl_check_const_ERR_STRING_DATA_lh_type(lh), out)
#define lh_ERR_STRING_DATA_stats_bio(lh, out) OPENSSL_LH_stats_bio(ossl_check_const_ERR_STRING_DATA_lh_type(lh), out)
#define lh_ERR_STRING_DATA_get_down_load(lh) OPENSSL_LH_get_down_load(ossl_check_ERR_STRING_DATA_lh_type(lh))
#define lh_ERR_STRING_DATA_set_down_load(lh, dl) OPENSSL_LH_set_down_load(ossl_check_ERR_STRING_DATA_lh_type(lh), dl)
#define lh_ERR_STRING_DATA_doall(lh, dfn) OPENSSL_LH_doall(ossl_check_ERR_STRING_DATA_lh_type(lh), ossl_check_ERR_STRING_DATA_lh_doallfunc_type(dfn))




#define ERR_MAX_DATA_SIZE 1024


void ERR_new(void);
void ERR_set_debug(const char *file, int line, const char *func);
void ERR_set_error(int lib, int reason, const char *fmt, ...);
void ERR_vset_error(int lib, int reason, const char *fmt, va_list args);


#define ERR_raise(lib, reason) ERR_raise_data((lib), (reason), NULL)
#define ERR_raise_data                                           \
    (ERR_new(),                                                  \
        ERR_set_debug(OPENSSL_FILE, OPENSSL_LINE, OPENSSL_FUNC), \
        ERR_set_error)

#ifndef OPENSSL_NO_DEPRECATED_3_0

#define ERR_put_error(lib, func, reason, file, line) \
    (ERR_new(),                                      \
        ERR_set_debug((file), (line), OPENSSL_FUNC), \
        ERR_set_error((lib), (reason), NULL))
#endif

void ERR_set_error_data(char *data, int flags);

unsigned long ERR_get_error(void);
unsigned long ERR_get_error_all(const char **file, int *line,
    const char **func,
    const char **data, int *flags);
#ifndef OPENSSL_NO_DEPRECATED_3_0
OSSL_DEPRECATEDIN_3_0
unsigned long ERR_get_error_line(const char **file, int *line);
OSSL_DEPRECATEDIN_3_0
unsigned long ERR_get_error_line_data(const char **file, int *line,
    const char **data, int *flags);
#endif
unsigned long ERR_peek_error(void);
unsigned long ERR_peek_error_line(const char **file, int *line);
unsigned long ERR_peek_error_func(const char **func);
unsigned long ERR_peek_error_data(const char **data, int *flags);
unsigned long ERR_peek_error_all(const char **file, int *line,
    const char **func,
    const char **data, int *flags);
#ifndef OPENSSL_NO_DEPRECATED_3_0
OSSL_DEPRECATEDIN_3_0
unsigned long ERR_peek_error_line_data(const char **file, int *line,
    const char **data, int *flags);
#endif
unsigned long ERR_peek_last_error(void);
unsigned long ERR_peek_last_error_line(const char **file, int *line);
unsigned long ERR_peek_last_error_func(const char **func);
unsigned long ERR_peek_last_error_data(const char **data, int *flags);
unsigned long ERR_peek_last_error_all(const char **file, int *line,
    const char **func,
    const char **data, int *flags);
#ifndef OPENSSL_NO_DEPRECATED_3_0
OSSL_DEPRECATEDIN_3_0
unsigned long ERR_peek_last_error_line_data(const char **file, int *line,
    const char **data, int *flags);
#endif

void ERR_clear_error(void);

char *ERR_error_string(unsigned long e, char *buf);
void ERR_error_string_n(unsigned long e, char *buf, size_t len);
const char *ERR_lib_error_string(unsigned long e);
#ifndef OPENSSL_NO_DEPRECATED_3_0
OSSL_DEPRECATEDIN_3_0 const char *ERR_func_error_string(unsigned long e);
#endif
const char *ERR_reason_error_string(unsigned long e);

void ERR_print_errors_cb(int (*cb)(const char *str, size_t len, void *u),
    void *u);
#ifndef OPENSSL_NO_STDIO
void ERR_print_errors_fp(FILE *fp);
#endif
void ERR_print_errors(BIO *bp);

void ERR_add_error_data(int num, ...);
void ERR_add_error_vdata(int num, va_list args);
void ERR_add_error_txt(const char *sepr, const char *txt);
void ERR_add_error_mem_bio(const char *sep, BIO *bio);

int ERR_load_strings(int lib, ERR_STRING_DATA *str);
int ERR_load_strings_const(const ERR_STRING_DATA *str);
int ERR_unload_strings(int lib, ERR_STRING_DATA *str);

#ifndef OPENSSL_NO_DEPRECATED_1_1_0
#define ERR_load_crypto_strings() \
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL)
#define ERR_free_strings() \
    while (0)              \
    continue
#endif

int ERR_get_next_error_library(void);

int ERR_set_mark(void);
int ERR_pop_to_mark(void);
int ERR_clear_last_mark(void);
int ERR_count_to_mark(void);
int ERR_pop(void);

ERR_STATE *OSSL_ERR_STATE_new(void);
void OSSL_ERR_STATE_save(ERR_STATE *es);
void OSSL_ERR_STATE_save_to_mark(ERR_STATE *es);
void OSSL_ERR_STATE_restore(const ERR_STATE *es);
void OSSL_ERR_STATE_free(ERR_STATE *es);

#ifdef __cplusplus
}
#endif

#endif

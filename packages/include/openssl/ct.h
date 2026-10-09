





#ifndef OPENSSL_CT_H
#define OPENSSL_CT_H
#pragma once

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_CT_H
#endif

#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_CT
#include <openssl/types.h>
#include <openssl/safestack.h>
#include <openssl/x509.h>
#include <openssl/cterr.h>
#ifdef __cplusplus
extern "C" {
#endif


#define SCT_MIN_RSA_BITS 2048


#define CT_V1_HASHLEN SHA256_DIGEST_LENGTH


SKM_DEFINE_STACK_OF_INTERNAL(SCT, SCT, SCT)
#define sk_SCT_num(sk) OPENSSL_sk_num(ossl_check_const_SCT_sk_type(sk))
#define sk_SCT_value(sk, idx) ((SCT *)OPENSSL_sk_value(ossl_check_const_SCT_sk_type(sk), (idx)))
#define sk_SCT_new(cmp) ((STACK_OF(SCT) *)OPENSSL_sk_set_cmp_thunks(OPENSSL_sk_new(ossl_check_SCT_compfunc_type(cmp)), sk_SCT_cmpfunc_thunk))
#define sk_SCT_new_null() ((STACK_OF(SCT) *)OPENSSL_sk_set_thunks(OPENSSL_sk_new_null(), sk_SCT_freefunc_thunk))
#define sk_SCT_new_reserve(cmp, n) ((STACK_OF(SCT) *)OPENSSL_sk_set_cmp_thunks(OPENSSL_sk_new_reserve(ossl_check_SCT_compfunc_type(cmp), (n)), sk_SCT_cmpfunc_thunk))
#define sk_SCT_reserve(sk, n) OPENSSL_sk_reserve(ossl_check_SCT_sk_type(sk), (n))
#define sk_SCT_free(sk) OPENSSL_sk_free(ossl_check_SCT_sk_type(sk))
#define sk_SCT_zero(sk) OPENSSL_sk_zero(ossl_check_SCT_sk_type(sk))
#define sk_SCT_delete(sk, i) ((SCT *)OPENSSL_sk_delete(ossl_check_SCT_sk_type(sk), (i)))
#define sk_SCT_delete_ptr(sk, ptr) ((SCT *)OPENSSL_sk_delete_ptr(ossl_check_SCT_sk_type(sk), ossl_check_SCT_type(ptr)))
#define sk_SCT_push(sk, ptr) OPENSSL_sk_push(ossl_check_SCT_sk_type(sk), ossl_check_SCT_type(ptr))
#define sk_SCT_unshift(sk, ptr) OPENSSL_sk_unshift(ossl_check_SCT_sk_type(sk), ossl_check_SCT_type(ptr))
#define sk_SCT_pop(sk) ((SCT *)OPENSSL_sk_pop(ossl_check_SCT_sk_type(sk)))
#define sk_SCT_shift(sk) ((SCT *)OPENSSL_sk_shift(ossl_check_SCT_sk_type(sk)))
#define sk_SCT_pop_free(sk, freefunc) OPENSSL_sk_pop_free(ossl_check_SCT_sk_type(sk), ossl_check_SCT_freefunc_type(freefunc))
#define sk_SCT_insert(sk, ptr, idx) OPENSSL_sk_insert(ossl_check_SCT_sk_type(sk), ossl_check_SCT_type(ptr), (idx))
#define sk_SCT_set(sk, idx, ptr) ((SCT *)OPENSSL_sk_set(ossl_check_SCT_sk_type(sk), (idx), ossl_check_SCT_type(ptr)))
#define sk_SCT_find(sk, ptr) OPENSSL_sk_find(ossl_check_SCT_sk_type(sk), ossl_check_SCT_type(ptr))
#define sk_SCT_find_ex(sk, ptr) OPENSSL_sk_find_ex(ossl_check_SCT_sk_type(sk), ossl_check_SCT_type(ptr))
#define sk_SCT_find_all(sk, ptr, pnum) OPENSSL_sk_find_all(ossl_check_SCT_sk_type(sk), ossl_check_SCT_type(ptr), pnum)
#define sk_SCT_sort(sk) OPENSSL_sk_sort(ossl_check_SCT_sk_type(sk))
#define sk_SCT_is_sorted(sk) OPENSSL_sk_is_sorted(ossl_check_const_SCT_sk_type(sk))
#define sk_SCT_dup(sk) ((STACK_OF(SCT) *)OPENSSL_sk_dup(ossl_check_const_SCT_sk_type(sk)))
#define sk_SCT_deep_copy(sk, copyfunc, freefunc) ((STACK_OF(SCT) *)OPENSSL_sk_deep_copy(ossl_check_const_SCT_sk_type(sk), ossl_check_SCT_copyfunc_type(copyfunc), ossl_check_SCT_freefunc_type(freefunc)))
#define sk_SCT_set_cmp_func(sk, cmp) ((sk_SCT_compfunc)OPENSSL_sk_set_cmp_func(ossl_check_SCT_sk_type(sk), ossl_check_SCT_compfunc_type(cmp)))
SKM_DEFINE_STACK_OF_INTERNAL(CTLOG, CTLOG, CTLOG)
#define sk_CTLOG_num(sk) OPENSSL_sk_num(ossl_check_const_CTLOG_sk_type(sk))
#define sk_CTLOG_value(sk, idx) ((CTLOG *)OPENSSL_sk_value(ossl_check_const_CTLOG_sk_type(sk), (idx)))
#define sk_CTLOG_new(cmp) ((STACK_OF(CTLOG) *)OPENSSL_sk_set_cmp_thunks(OPENSSL_sk_new(ossl_check_CTLOG_compfunc_type(cmp)), sk_CTLOG_cmpfunc_thunk))
#define sk_CTLOG_new_null() ((STACK_OF(CTLOG) *)OPENSSL_sk_set_thunks(OPENSSL_sk_new_null(), sk_CTLOG_freefunc_thunk))
#define sk_CTLOG_new_reserve(cmp, n) ((STACK_OF(CTLOG) *)OPENSSL_sk_set_cmp_thunks(OPENSSL_sk_new_reserve(ossl_check_CTLOG_compfunc_type(cmp), (n)), sk_CTLOG_cmpfunc_thunk))
#define sk_CTLOG_reserve(sk, n) OPENSSL_sk_reserve(ossl_check_CTLOG_sk_type(sk), (n))
#define sk_CTLOG_free(sk) OPENSSL_sk_free(ossl_check_CTLOG_sk_type(sk))
#define sk_CTLOG_zero(sk) OPENSSL_sk_zero(ossl_check_CTLOG_sk_type(sk))
#define sk_CTLOG_delete(sk, i) ((CTLOG *)OPENSSL_sk_delete(ossl_check_CTLOG_sk_type(sk), (i)))
#define sk_CTLOG_delete_ptr(sk, ptr) ((CTLOG *)OPENSSL_sk_delete_ptr(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_type(ptr)))
#define sk_CTLOG_push(sk, ptr) OPENSSL_sk_push(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_type(ptr))
#define sk_CTLOG_unshift(sk, ptr) OPENSSL_sk_unshift(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_type(ptr))
#define sk_CTLOG_pop(sk) ((CTLOG *)OPENSSL_sk_pop(ossl_check_CTLOG_sk_type(sk)))
#define sk_CTLOG_shift(sk) ((CTLOG *)OPENSSL_sk_shift(ossl_check_CTLOG_sk_type(sk)))
#define sk_CTLOG_pop_free(sk, freefunc) OPENSSL_sk_pop_free(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_freefunc_type(freefunc))
#define sk_CTLOG_insert(sk, ptr, idx) OPENSSL_sk_insert(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_type(ptr), (idx))
#define sk_CTLOG_set(sk, idx, ptr) ((CTLOG *)OPENSSL_sk_set(ossl_check_CTLOG_sk_type(sk), (idx), ossl_check_CTLOG_type(ptr)))
#define sk_CTLOG_find(sk, ptr) OPENSSL_sk_find(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_type(ptr))
#define sk_CTLOG_find_ex(sk, ptr) OPENSSL_sk_find_ex(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_type(ptr))
#define sk_CTLOG_find_all(sk, ptr, pnum) OPENSSL_sk_find_all(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_type(ptr), pnum)
#define sk_CTLOG_sort(sk) OPENSSL_sk_sort(ossl_check_CTLOG_sk_type(sk))
#define sk_CTLOG_is_sorted(sk) OPENSSL_sk_is_sorted(ossl_check_const_CTLOG_sk_type(sk))
#define sk_CTLOG_dup(sk) ((STACK_OF(CTLOG) *)OPENSSL_sk_dup(ossl_check_const_CTLOG_sk_type(sk)))
#define sk_CTLOG_deep_copy(sk, copyfunc, freefunc) ((STACK_OF(CTLOG) *)OPENSSL_sk_deep_copy(ossl_check_const_CTLOG_sk_type(sk), ossl_check_CTLOG_copyfunc_type(copyfunc), ossl_check_CTLOG_freefunc_type(freefunc)))
#define sk_CTLOG_set_cmp_func(sk, cmp) ((sk_CTLOG_compfunc)OPENSSL_sk_set_cmp_func(ossl_check_CTLOG_sk_type(sk), ossl_check_CTLOG_compfunc_type(cmp)))



typedef enum {
    CT_LOG_ENTRY_TYPE_NOT_SET = -1,
    CT_LOG_ENTRY_TYPE_X509 = 0,
    CT_LOG_ENTRY_TYPE_PRECERT = 1
} ct_log_entry_type_t;

typedef enum {
    SCT_VERSION_NOT_SET = -1,
    SCT_VERSION_V1 = 0
} sct_version_t;

typedef enum {
    SCT_SOURCE_UNKNOWN,
    SCT_SOURCE_TLS_EXTENSION,
    SCT_SOURCE_X509V3_EXTENSION,
    SCT_SOURCE_OCSP_STAPLED_RESPONSE
} sct_source_t;

typedef enum {
    SCT_VALIDATION_STATUS_NOT_SET,
    SCT_VALIDATION_STATUS_UNKNOWN_LOG,
    SCT_VALIDATION_STATUS_VALID,
    SCT_VALIDATION_STATUS_INVALID,
    SCT_VALIDATION_STATUS_UNVERIFIED,
    SCT_VALIDATION_STATUS_UNKNOWN_VERSION
} sct_validation_status_t;




CT_POLICY_EVAL_CTX *CT_POLICY_EVAL_CTX_new_ex(OSSL_LIB_CTX *libctx,
    const char *propq);


CT_POLICY_EVAL_CTX *CT_POLICY_EVAL_CTX_new(void);


void CT_POLICY_EVAL_CTX_free(CT_POLICY_EVAL_CTX *ctx);


X509 *CT_POLICY_EVAL_CTX_get0_cert(const CT_POLICY_EVAL_CTX *ctx);


int CT_POLICY_EVAL_CTX_set1_cert(CT_POLICY_EVAL_CTX *ctx, X509 *cert);


X509 *CT_POLICY_EVAL_CTX_get0_issuer(const CT_POLICY_EVAL_CTX *ctx);


int CT_POLICY_EVAL_CTX_set1_issuer(CT_POLICY_EVAL_CTX *ctx, X509 *issuer);


const CTLOG_STORE *CT_POLICY_EVAL_CTX_get0_log_store(const CT_POLICY_EVAL_CTX *ctx);


void CT_POLICY_EVAL_CTX_set_shared_CTLOG_STORE(CT_POLICY_EVAL_CTX *ctx,
    CTLOG_STORE *log_store);


uint64_t CT_POLICY_EVAL_CTX_get_time(const CT_POLICY_EVAL_CTX *ctx);


void CT_POLICY_EVAL_CTX_set_time(CT_POLICY_EVAL_CTX *ctx, uint64_t time_in_ms);




SCT *SCT_new(void);


SCT *SCT_new_from_base64(unsigned char version,
    const char *logid_base64,
    ct_log_entry_type_t entry_type,
    uint64_t timestamp,
    const char *extensions_base64,
    const char *signature_base64);


void SCT_free(SCT *sct);


void SCT_LIST_free(STACK_OF(SCT) *a);


sct_version_t SCT_get_version(const SCT *sct);


__owur int SCT_set_version(SCT *sct, sct_version_t version);


ct_log_entry_type_t SCT_get_log_entry_type(const SCT *sct);


__owur int SCT_set_log_entry_type(SCT *sct, ct_log_entry_type_t entry_type);


size_t SCT_get0_log_id(const SCT *sct, unsigned char **log_id);


__owur int SCT_set0_log_id(SCT *sct, unsigned char *log_id, size_t log_id_len);


__owur int SCT_set1_log_id(SCT *sct, const unsigned char *log_id,
    size_t log_id_len);


uint64_t SCT_get_timestamp(const SCT *sct);


void SCT_set_timestamp(SCT *sct, uint64_t timestamp);


int SCT_get_signature_nid(const SCT *sct);


__owur int SCT_set_signature_nid(SCT *sct, int nid);


size_t SCT_get0_extensions(const SCT *sct, unsigned char **ext);


void SCT_set0_extensions(SCT *sct, unsigned char *ext, size_t ext_len);


__owur int SCT_set1_extensions(SCT *sct, const unsigned char *ext,
    size_t ext_len);


size_t SCT_get0_signature(const SCT *sct, unsigned char **sig);


void SCT_set0_signature(SCT *sct, unsigned char *sig, size_t sig_len);


__owur int SCT_set1_signature(SCT *sct, const unsigned char *sig,
    size_t sig_len);


sct_source_t SCT_get_source(const SCT *sct);


__owur int SCT_set_source(SCT *sct, sct_source_t source);


const char *SCT_validation_status_string(const SCT *sct);


void SCT_print(const SCT *sct, BIO *out, int indent, const CTLOG_STORE *logs);


void SCT_LIST_print(const STACK_OF(SCT) *sct_list, BIO *out, int indent,
    const char *separator, const CTLOG_STORE *logs);


sct_validation_status_t SCT_get_validation_status(const SCT *sct);


__owur int SCT_validate(SCT *sct, const CT_POLICY_EVAL_CTX *ctx);


__owur int SCT_LIST_validate(const STACK_OF(SCT) *scts,
    CT_POLICY_EVAL_CTX *ctx);




__owur int i2o_SCT_LIST(const STACK_OF(SCT) *a, unsigned char **pp);


STACK_OF(SCT) *o2i_SCT_LIST(STACK_OF(SCT) **a, const unsigned char **pp,
    size_t len);


__owur int i2d_SCT_LIST(const STACK_OF(SCT) *a, unsigned char **pp);


STACK_OF(SCT) *d2i_SCT_LIST(STACK_OF(SCT) **a, const unsigned char **pp,
    long len);


__owur int i2o_SCT(const SCT *sct, unsigned char **out);


SCT *o2i_SCT(SCT **psct, const unsigned char **in, size_t len);




CTLOG *CTLOG_new_ex(EVP_PKEY *public_key, const char *name, OSSL_LIB_CTX *libctx,
    const char *propq);


CTLOG *CTLOG_new(EVP_PKEY *public_key, const char *name);


int CTLOG_new_from_base64_ex(CTLOG **ct_log, const char *pkey_base64,
    const char *name, OSSL_LIB_CTX *libctx,
    const char *propq);


int CTLOG_new_from_base64(CTLOG **ct_log,
    const char *pkey_base64, const char *name);


void CTLOG_free(CTLOG *log);


const char *CTLOG_get0_name(const CTLOG *log);

void CTLOG_get0_log_id(const CTLOG *log, const uint8_t **log_id,
    size_t *log_id_len);

EVP_PKEY *CTLOG_get0_public_key(const CTLOG *log);




CTLOG_STORE *CTLOG_STORE_new_ex(OSSL_LIB_CTX *libctx, const char *propq);


CTLOG_STORE *CTLOG_STORE_new(void);


void CTLOG_STORE_free(CTLOG_STORE *store);


const CTLOG *CTLOG_STORE_get0_log_by_id(const CTLOG_STORE *store,
    const uint8_t *log_id,
    size_t log_id_len);


__owur int CTLOG_STORE_load_file(CTLOG_STORE *store, const char *file);


__owur int CTLOG_STORE_load_default_file(CTLOG_STORE *store);

#ifdef __cplusplus
}
#endif
#endif
#endif

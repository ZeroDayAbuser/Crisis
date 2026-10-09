








#ifndef OPENSSL_CORE_H
#define OPENSSL_CORE_H
#pragma once

#include <stddef.h>
#include <openssl/types.h>

#ifdef __cplusplus
extern "C" {
#endif










typedef struct ossl_core_handle_st OSSL_CORE_HANDLE;
typedef struct openssl_core_ctx_st OPENSSL_CORE_CTX;
typedef struct ossl_core_bio_st OSSL_CORE_BIO;







struct ossl_dispatch_st {
    int function_id;
    void (*function)(void);
};

#define OSSL_DISPATCH_END \
    { 0, NULL }














struct ossl_item_st {
    unsigned int id;
    void *ptr;
};







struct ossl_algorithm_st {
    const char *algorithm_names; 
    const char *property_definition; 
    const OSSL_DISPATCH *implementation;
    const char *algorithm_description;
};







struct ossl_param_st {
    const char *key; 
    unsigned int data_type; 
    void *data; 
    size_t data_size; 
    size_t return_size; 
};














#define OSSL_PARAM_INTEGER 1
#define OSSL_PARAM_UNSIGNED_INTEGER 2




#define OSSL_PARAM_REAL 3




#define OSSL_PARAM_UTF8_STRING 4





#define OSSL_PARAM_OCTET_STRING 5

















#define OSSL_PARAM_UTF8_PTR 6


















#define OSSL_PARAM_OCTET_PTR 7











typedef void (*OSSL_thread_stop_handler_fn)(void *arg);



















typedef int(OSSL_provider_init_fn)(const OSSL_CORE_HANDLE *handle,
    const OSSL_DISPATCH *in,
    const OSSL_DISPATCH **out,
    void **provctx);
#ifdef __VMS
#pragma names save
#pragma names uppercase, truncated
#endif
OPENSSL_EXPORT OSSL_provider_init_fn OSSL_provider_init;
#ifdef __VMS
#pragma names restore
#endif














typedef int(OSSL_CALLBACK)(const OSSL_PARAM params[], void *arg);
typedef int(OSSL_INOUT_CALLBACK)(const OSSL_PARAM in_params[],
    OSSL_PARAM out_params[], void *arg);






typedef int(OSSL_PASSPHRASE_CALLBACK)(char *pass, size_t pass_size,
    size_t *pass_len,
    const OSSL_PARAM params[], void *arg);

#ifdef __cplusplus
}
#endif

#endif



#ifndef OPENSSL_INDICATOR_H
#define OPENSSL_INDICATOR_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/params.h>

typedef int(OSSL_INDICATOR_CALLBACK)(const char *type, const char *desc,
    const OSSL_PARAM params[]);

void OSSL_INDICATOR_set_callback(OSSL_LIB_CTX *libctx,
    OSSL_INDICATOR_CALLBACK *cb);
void OSSL_INDICATOR_get_callback(OSSL_LIB_CTX *libctx,
    OSSL_INDICATOR_CALLBACK **cb);

#ifdef __cplusplus
}
#endif
#endif 

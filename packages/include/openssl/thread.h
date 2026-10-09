









#ifndef OPENSSL_THREAD_H
#define OPENSSL_THREAD_H

#define OSSL_THREAD_SUPPORT_FLAG_THREAD_POOL (1U << 0)
#define OSSL_THREAD_SUPPORT_FLAG_DEFAULT_SPAWN (1U << 1)

#include <openssl/types.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t OSSL_get_thread_support_flags(void);
int OSSL_set_max_threads(OSSL_LIB_CTX *ctx, uint64_t max_threads);
uint64_t OSSL_get_max_threads(OSSL_LIB_CTX *ctx);

#ifdef __cplusplus
}
#endif

#endif 

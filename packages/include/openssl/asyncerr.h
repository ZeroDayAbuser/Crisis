

#ifndef OPENSSL_ASYNCERR_H
#define OPENSSL_ASYNCERR_H
#pragma once

#include <openssl/opensslconf.h>
#include <openssl/symhacks.h>
#include <openssl/cryptoerr_legacy.h>


#define ASYNC_R_FAILED_TO_SET_POOL 101
#define ASYNC_R_FAILED_TO_SWAP_CONTEXT 102
#define ASYNC_R_INIT_FAILED 105
#define ASYNC_R_INVALID_POOL_SIZE 103

#endif

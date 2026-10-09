
#ifndef OPENSSL_COMPERR_H
#define OPENSSL_COMPERR_H
#pragma once

#include <openssl/opensslconf.h>
#include <openssl/symhacks.h>
#include <openssl/cryptoerr_legacy.h>

#ifndef OPENSSL_NO_COMP

#define COMP_R_BROTLI_DECODE_ERROR 102
#define COMP_R_BROTLI_ENCODE_ERROR 103
#define COMP_R_BROTLI_NOT_SUPPORTED 104
#define COMP_R_ZLIB_DEFLATE_ERROR 99
#define COMP_R_ZLIB_INFLATE_ERROR 100
#define COMP_R_ZLIB_NOT_SUPPORTED 101
#define COMP_R_ZSTD_COMPRESS_ERROR 105
#define COMP_R_ZSTD_DECODE_ERROR 106
#define COMP_R_ZSTD_DECOMPRESS_ERROR 107
#define COMP_R_ZSTD_NOT_SUPPORTED 108

#endif
#endif

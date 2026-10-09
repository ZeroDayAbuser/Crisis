
#ifndef OPENSSL_DSAERR_H
#define OPENSSL_DSAERR_H
#pragma once

#include <openssl/opensslconf.h>
#include <openssl/symhacks.h>
#include <openssl/cryptoerr_legacy.h>

#ifndef OPENSSL_NO_DSA

#define DSA_R_BAD_FFC_PARAMETERS 114
#define DSA_R_BAD_Q_VALUE 102
#define DSA_R_BN_DECODE_ERROR 108
#define DSA_R_BN_ERROR 109
#define DSA_R_DECODE_ERROR 104
#define DSA_R_INVALID_DIGEST_TYPE 106
#define DSA_R_INVALID_PARAMETERS 112
#define DSA_R_MISSING_PARAMETERS 101
#define DSA_R_MISSING_PRIVATE_KEY 111
#define DSA_R_MODULUS_TOO_LARGE 103
#define DSA_R_NO_PARAMETERS_SET 107
#define DSA_R_PARAMETER_ENCODING_ERROR 105
#define DSA_R_P_NOT_PRIME 115
#define DSA_R_Q_NOT_PRIME 113
#define DSA_R_SEED_LEN_SMALL 110
#define DSA_R_TOO_MANY_RETRIES 116

#endif
#endif

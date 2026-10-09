

#ifndef OPENSSL_PROV_SSL_H
#define OPENSSL_PROV_SSL_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SSL_MAX_MASTER_KEY_LENGTH 48

#define SSL3_VERSION 0x0300
#define TLS1_VERSION 0x0301
#define TLS1_1_VERSION 0x0302
#define TLS1_2_VERSION 0x0303
#define TLS1_3_VERSION 0x0304
#define DTLS1_VERSION 0xFEFF
#define DTLS1_2_VERSION 0xFEFD
#define DTLS1_BAD_VER 0x0100

#define OSSL_QUIC1_VERSION 0x0000001

#ifdef __cplusplus
}
#endif
#endif

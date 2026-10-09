

#ifndef OPENSSL_OPENSSLV_H
#define OPENSSL_OPENSSLV_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif





# define OPENSSL_VERSION_MAJOR  4


# define OPENSSL_VERSION_MINOR  0


# define OPENSSL_VERSION_PATCH  1






# define OPENSSL_VERSION_PRE_RELEASE ""




# define OPENSSL_VERSION_BUILD_METADATA ""






# define OPENSSL_SHLIB_VERSION 4





#define OPENSSL_VERSION_PREREQ(maj, min) \
    ((OPENSSL_VERSION_MAJOR << 16) + OPENSSL_VERSION_MINOR >= ((maj) << 16) + (min))



# define OPENSSL_VERSION_STR "4.0.1"


# define OPENSSL_FULL_VERSION_STR "4.0.1"




# define OPENSSL_RELEASE_DATE "9 Jun 2026"





# define OPENSSL_VERSION_TEXT "OpenSSL 4.0.1 9 Jun 2026"




# define OPENSSL_VERSION_NUMBER          \
    ( (OPENSSL_VERSION_MAJOR<<28)        \
      |(OPENSSL_VERSION_MINOR<<20)       \
      |(OPENSSL_VERSION_PATCH<<4)        \
      |0x0L )


#ifdef __cplusplus
}
#endif

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_OPENSSLV_H
#endif

#endif 

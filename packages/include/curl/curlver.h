#ifndef CURLINC_CURLVER_H
#define CURLINC_CURLVER_H





#define LIBCURL_COPYRIGHT "Daniel Stenberg, <daniel@haxx.se>."


#define LIBCURL_VERSION "8.22.0-DEV"


#define LIBCURL_VERSION_MAJOR 8
#define LIBCURL_VERSION_MINOR 22
#define LIBCURL_VERSION_PATCH 0

#define LIBCURL_VERSION_NUM 0x081600


#define LIBCURL_TIMESTAMP "[unreleased]"

#define CURL_VERSION_BITS(x, y, z) ((x) << 16 | (y) << 8 | (z))
#define CURL_AT_LEAST_VERSION(x, y, z) \
  (LIBCURL_VERSION_NUM >= CURL_VERSION_BITS(x, y, z))

#endif 

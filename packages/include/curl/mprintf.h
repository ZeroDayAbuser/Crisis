#ifndef CURLINC_MPRINTF_H
#define CURLINC_MPRINTF_H

#include <stdarg.h>
#include <stdio.h>
#include "curl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CURL_TEMP_PRINTF
#if (defined(__GNUC__) || defined(__clang__) ||                         \
  defined(__IAR_SYSTEMS_ICC__)) &&                                      \
  defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) &&         \
  !defined(CURL_NO_FMT_CHECKS)
#if defined(__MINGW32__) && !defined(__clang__)
#ifdef __MINGW_PRINTF_FORMAT
#define CURL_TEMP_PRINTF(fmt, arg) \
  __attribute__((format(__MINGW_PRINTF_FORMAT, fmt, arg)))
#else
#define CURL_TEMP_PRINTF(fmt, arg)
#endif
#else
#define CURL_TEMP_PRINTF(fmt, arg) \
  __attribute__((format(printf, fmt, arg)))
#endif
#else
#define CURL_TEMP_PRINTF(fmt, arg)
#endif
#endif

CURL_EXTERN int curl_mprintf(const char *format, ...)
  CURL_TEMP_PRINTF(1, 2);
CURL_EXTERN int curl_mfprintf(FILE *fd, const char *format, ...)
  CURL_TEMP_PRINTF(2, 3);
CURL_EXTERN int curl_msprintf(char *buffer, const char *format, ...)
  CURL_TEMP_PRINTF(2, 3);
CURL_EXTERN int curl_msnprintf(char *buffer, size_t maxlength,
                               const char *format, ...)
  CURL_TEMP_PRINTF(3, 4);
CURL_EXTERN int curl_mvprintf(const char *format, va_list args)
  CURL_TEMP_PRINTF(1, 0);
CURL_EXTERN int curl_mvfprintf(FILE *fd, const char *format, va_list args)
  CURL_TEMP_PRINTF(2, 0);
CURL_EXTERN int curl_mvsprintf(char *buffer, const char *format, va_list args)
  CURL_TEMP_PRINTF(2, 0);
CURL_EXTERN int curl_mvsnprintf(char *buffer, size_t maxlength,
                                const char *format, va_list args)
  CURL_TEMP_PRINTF(3, 0);
CURL_EXTERN char *curl_maprintf(const char *format, ...)
  CURL_TEMP_PRINTF(1, 2);
CURL_EXTERN char *curl_mvaprintf(const char *format, va_list args)
  CURL_TEMP_PRINTF(1, 0);

#undef CURL_TEMP_PRINTF

#ifdef __cplusplus
}
#endif

#endif

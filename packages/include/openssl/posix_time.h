




#ifndef OPENSSL_HEADER_POSIX_TIME_H
#define OPENSSL_HEADER_POSIX_TIME_H
#include <stdint.h>
#include <time.h>

#if defined(__cplusplus)
extern "C" {
#endif




int OPENSSL_posix_to_tm(int64_t time, struct tm *out_tm);




int OPENSSL_tm_to_posix(const struct tm *tm, int64_t *out);




int OPENSSL_timegm(const struct tm *tm, time_t *out);

#if defined(__cplusplus)
}
#endif

#endif

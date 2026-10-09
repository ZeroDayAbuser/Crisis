#ifndef CURLINC_SYSTEM_H
#define CURLINC_SYSTEM_H

#ifdef __DJGPP__
#  define CURL_TYPEOF_CURL_OFF_T     long long
#  define CURL_FORMAT_CURL_OFF_T     "lld"
#  define CURL_FORMAT_CURL_OFF_TU    "llu"
#  define CURL_SUFFIX_CURL_OFF_T     LL
#  define CURL_SUFFIX_CURL_OFF_TU    ULL
#  define CURL_TYPEOF_CURL_SOCKLEN_T int

#elif defined(__BORLANDC__)
#  define CURL_TYPEOF_CURL_OFF_T     __int64
#  define CURL_FORMAT_CURL_OFF_T     "I64d"
#  define CURL_FORMAT_CURL_OFF_TU    "I64u"
#  define CURL_SUFFIX_CURL_OFF_T     i64
#  define CURL_SUFFIX_CURL_OFF_TU    ui64
#  define CURL_TYPEOF_CURL_SOCKLEN_T int

#elif defined(__POCC__)
#  ifdef _MSC_VER
#    define CURL_TYPEOF_CURL_OFF_T     __int64
#    define CURL_FORMAT_CURL_OFF_T     "I64d"
#    define CURL_FORMAT_CURL_OFF_TU    "I64u"
#    define CURL_SUFFIX_CURL_OFF_T     i64
#    define CURL_SUFFIX_CURL_OFF_TU    ui64
#  else
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#  endif
#  define CURL_TYPEOF_CURL_SOCKLEN_T int

#elif defined(__LCC__)
#  ifdef __MCST__
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#    define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#    define CURL_PULL_SYS_TYPES_H      1
#    define CURL_PULL_SYS_SOCKET_H     1
#  else
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#    define CURL_TYPEOF_CURL_SOCKLEN_T int
#  endif

#elif defined(macintosh)
#  include <ConditionalMacros.h>
#  if TYPE_LONGLONG
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#  else
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#  endif
#  define CURL_TYPEOF_CURL_SOCKLEN_T unsigned int

#elif defined(__TANDEM)
#  ifndef __LP64
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#    define CURL_TYPEOF_CURL_SOCKLEN_T int
#  else
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#    define CURL_TYPEOF_CURL_SOCKLEN_T unsigned int
#  endif

#elif defined(UNDER_CE)
#  define CURL_TYPEOF_CURL_OFF_T     __int64
#  define CURL_FORMAT_CURL_OFF_T     "I64d"
#  define CURL_FORMAT_CURL_OFF_TU    "I64u"
#  define CURL_SUFFIX_CURL_OFF_T     i64
#  define CURL_SUFFIX_CURL_OFF_TU    ui64
#  define CURL_TYPEOF_CURL_SOCKLEN_T int

#elif defined(__MINGW32__)
#  include <inttypes.h>
#  define CURL_TYPEOF_CURL_OFF_T     long long
#  define CURL_FORMAT_CURL_OFF_T     PRId64
#  define CURL_FORMAT_CURL_OFF_TU    PRIu64
#  define CURL_SUFFIX_CURL_OFF_T     LL
#  define CURL_SUFFIX_CURL_OFF_TU    ULL
#  define CURL_TYPEOF_CURL_SOCKLEN_T int
#  define CURL_PULL_SYS_TYPES_H      1

#elif defined(__VMS)
#  ifdef __VAX
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#  else
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#  endif
#  define CURL_TYPEOF_CURL_SOCKLEN_T unsigned int

#elif defined(__OS400__)
#  define CURL_TYPEOF_CURL_OFF_T     long long
#  define CURL_FORMAT_CURL_OFF_T     "lld"
#  define CURL_FORMAT_CURL_OFF_TU    "llu"
#  define CURL_SUFFIX_CURL_OFF_T     LL
#  define CURL_SUFFIX_CURL_OFF_TU    ULL
#  define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#  define CURL_PULL_SYS_TYPES_H      1
#  define CURL_PULL_SYS_SOCKET_H     1

#elif defined(__MVS__)
#  ifdef _LONG_LONG
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#  else
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#  endif
#  define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#  define CURL_PULL_SYS_TYPES_H      1
#  define CURL_PULL_SYS_SOCKET_H     1

#elif defined(__370__)
#  if defined(__IBMC__) || defined(__IBMCPP__)
#    ifdef _LONG_LONG
#      define CURL_TYPEOF_CURL_OFF_T     long long
#      define CURL_FORMAT_CURL_OFF_T     "lld"
#      define CURL_FORMAT_CURL_OFF_TU    "llu"
#      define CURL_SUFFIX_CURL_OFF_T     LL
#      define CURL_SUFFIX_CURL_OFF_TU    ULL
#    else
#      define CURL_TYPEOF_CURL_OFF_T     long
#      define CURL_FORMAT_CURL_OFF_T     "ld"
#      define CURL_FORMAT_CURL_OFF_TU    "lu"
#      define CURL_SUFFIX_CURL_OFF_T     L
#      define CURL_SUFFIX_CURL_OFF_TU    UL
#    endif
#    define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#    define CURL_PULL_SYS_TYPES_H      1
#    define CURL_PULL_SYS_SOCKET_H     1
#  endif

#elif defined(TPF)
#  define CURL_TYPEOF_CURL_OFF_T     long
#  define CURL_FORMAT_CURL_OFF_T     "ld"
#  define CURL_FORMAT_CURL_OFF_TU    "lu"
#  define CURL_SUFFIX_CURL_OFF_T     L
#  define CURL_SUFFIX_CURL_OFF_TU    UL
#  define CURL_TYPEOF_CURL_SOCKLEN_T int

#elif defined(__TINYC__)
#  define CURL_TYPEOF_CURL_OFF_T     long long
#  define CURL_FORMAT_CURL_OFF_T     "lld"
#  define CURL_FORMAT_CURL_OFF_TU    "llu"
#  define CURL_SUFFIX_CURL_OFF_T     LL
#  define CURL_SUFFIX_CURL_OFF_TU    ULL
#  define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#  define CURL_PULL_SYS_TYPES_H      1
#  define CURL_PULL_SYS_SOCKET_H     1

#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#  if !defined(__LP64) && (defined(__ILP32) ||                          \
                           defined(__i386) ||                           \
                           defined(__sparcv8) ||                        \
                           defined(__sparcv8plus))
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#  elif defined(__LP64) || \
        defined(__amd64) || defined(__sparcv9)
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#  endif
#  define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#  define CURL_PULL_SYS_TYPES_H      1
#  define CURL_PULL_SYS_SOCKET_H     1

#elif defined(__xlc__)
#  ifndef _LP64
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#  else
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#  endif
#  define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#  define CURL_PULL_SYS_TYPES_H      1
#  define CURL_PULL_SYS_SOCKET_H     1

#elif defined(__hpux)
#  ifndef _LP64
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#  else
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#  endif
#  define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#  define CURL_PULL_SYS_TYPES_H      1
#  define CURL_PULL_SYS_SOCKET_H     1

#elif defined(_MSC_VER)
#  if _MSC_VER >= 1800
#    include <inttypes.h>
#    define CURL_FORMAT_CURL_OFF_T     PRId64
#    define CURL_FORMAT_CURL_OFF_TU    PRIu64
#  else
#    define CURL_FORMAT_CURL_OFF_T     "I64d"
#    define CURL_FORMAT_CURL_OFF_TU    "I64u"
#  endif
#  define CURL_TYPEOF_CURL_OFF_T     __int64
#  define CURL_SUFFIX_CURL_OFF_T     i64
#  define CURL_SUFFIX_CURL_OFF_TU    ui64
#  define CURL_TYPEOF_CURL_SOCKLEN_T int

#elif defined(__GNUC__) && !defined(_SCO_DS)
#  if !defined(__LP64__) &&                                             \
  (defined(__ILP32__) || defined(__i386__) || defined(__hppa__) ||      \
   defined(__ppc__) || defined(__powerpc__) || defined(__arm__) ||      \
   defined(__sparc__) || defined(__mips__) || defined(__sh__) ||        \
   defined(__XTENSA__) ||                                               \
   (defined(__SIZEOF_LONG__) && __SIZEOF_LONG__ == 4) ||                \
   (defined(__LONG_MAX__) && __LONG_MAX__ == 2147483647L))
#    define CURL_TYPEOF_CURL_OFF_T     long long
#    define CURL_FORMAT_CURL_OFF_T     "lld"
#    define CURL_FORMAT_CURL_OFF_TU    "llu"
#    define CURL_SUFFIX_CURL_OFF_T     LL
#    define CURL_SUFFIX_CURL_OFF_TU    ULL
#    if (__GNUC__ >= 4) || \
       ((__GNUC__ == 3) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__ >= 4))
#      define CURL_POPCOUNT64(x)       __builtin_popcountll(x)
#      define CURL_CTZ64(x)            __builtin_ctzll(x)
#    endif
#  elif defined(__LP64__) || \
        defined(__x86_64__) || defined(__ppc64__) || defined(__sparc64__) || \
        defined(__e2k__) || \
        (defined(__SIZEOF_LONG__) && __SIZEOF_LONG__ == 8) || \
        (defined(__LONG_MAX__) && __LONG_MAX__ == 9223372036854775807L)
#    define CURL_TYPEOF_CURL_OFF_T     long
#    define CURL_FORMAT_CURL_OFF_T     "ld"
#    define CURL_FORMAT_CURL_OFF_TU    "lu"
#    define CURL_SUFFIX_CURL_OFF_T     L
#    define CURL_SUFFIX_CURL_OFF_TU    UL
#    if (__GNUC__ >= 4) || \
       ((__GNUC__ == 3) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__ >= 4))
#      define CURL_POPCOUNT64(x)       __builtin_popcountl(x)
#      define CURL_CTZ64(x)            __builtin_ctzl(x)
#    endif
#  endif
#  define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
#  define CURL_PULL_SYS_TYPES_H      1
#  define CURL_PULL_SYS_SOCKET_H     1

#else
#  define CURL_TYPEOF_CURL_OFF_T     long long
#  define CURL_FORMAT_CURL_OFF_T     "lld"
#  define CURL_FORMAT_CURL_OFF_TU    "llu"
#  define CURL_SUFFIX_CURL_OFF_T     LL
#  define CURL_SUFFIX_CURL_OFF_TU    ULL
#  define CURL_TYPEOF_CURL_SOCKLEN_T int
#endif

#ifdef _AIX
#define CURL_PULL_SYS_POLL_H
#endif

#ifdef CURL_PULL_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef CURL_PULL_SYS_SOCKET_H
#  include <sys/socket.h>
#endif

#ifdef CURL_PULL_SYS_POLL_H
#  include <sys/poll.h>
#endif

#ifdef CURL_TYPEOF_CURL_SOCKLEN_T
  typedef CURL_TYPEOF_CURL_SOCKLEN_T curl_socklen_t;
#endif

#ifdef CURL_TYPEOF_CURL_OFF_T
  typedef CURL_TYPEOF_CURL_OFF_T curl_off_t;
#endif

#endif

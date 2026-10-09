






























#ifndef ZYDIS_DEFINES_H
#define ZYDIS_DEFINES_H

#include <Zycore/Defines.h>













#if defined(ZYDIS_STATIC_DEFINE)
#   pragma message("ZYDIS_STATIC_DEFINE was renamed to ZYDIS_STATIC_BUILD.")
#   define ZYDIS_STATIC_BUILD
#endif
#if defined(Zydis_EXPORTS)
#   pragma message("Zydis_EXPORTS was renamed to ZYDIS_SHOULD_EXPORT.")
#   define ZYDIS_SHOULD_EXPORT
#endif




#if defined(ZYDIS_STATIC_BUILD)
#   define ZYDIS_EXPORT
#else
#   if defined(ZYDIS_SHOULD_EXPORT)
#       define ZYDIS_EXPORT ZYAN_DLLEXPORT
#   else
#       define ZYDIS_EXPORT ZYAN_DLLIMPORT
#   endif
#endif




#define ZYDIS_NO_EXPORT

#endif 

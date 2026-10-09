
#ifndef ZYCORE_API_MEMORY_H
#define ZYCORE_API_MEMORY_H

#include <Zycore/Defines.h>
#include <Zycore/Status.h>
#include <Zycore/Types.h>

#ifndef ZYAN_NO_LIBC

#if   defined(ZYAN_WINDOWS)
#   include <windows.h>
#elif defined(ZYAN_POSIX)
#   include <sys/mman.h>
#else
#   error "Unsupported platform detected"
#endif

typedef enum ZyanMemoryPageProtection_
{
#if   defined(ZYAN_WINDOWS)

    ZYAN_PAGE_READONLY          = PAGE_READONLY,
    ZYAN_PAGE_READWRITE         = PAGE_READWRITE,
    ZYAN_PAGE_EXECUTE           = PAGE_EXECUTE,
    ZYAN_PAGE_EXECUTE_READ      = PAGE_EXECUTE_READ,
    ZYAN_PAGE_EXECUTE_READWRITE = PAGE_EXECUTE_READWRITE

#elif defined(ZYAN_POSIX)

    ZYAN_PAGE_READONLY          = PROT_READ,
    ZYAN_PAGE_READWRITE         = PROT_READ | PROT_WRITE,
    ZYAN_PAGE_EXECUTE           = PROT_EXEC,
    ZYAN_PAGE_EXECUTE_READ      = PROT_EXEC | PROT_READ,
    ZYAN_PAGE_EXECUTE_READWRITE = PROT_EXEC | PROT_READ | PROT_WRITE

#endif
} ZyanMemoryPageProtection;

ZYCORE_EXPORT ZyanU32 ZyanMemoryGetSystemPageSize(void);

ZYCORE_EXPORT ZyanU32 ZyanMemoryGetSystemAllocationGranularity(void);

ZYCORE_EXPORT ZyanStatus ZyanMemoryVirtualProtect(void* address, ZyanUSize size,
    ZyanMemoryPageProtection protection);

ZYCORE_EXPORT ZyanStatus ZyanMemoryVirtualFree(void* address, ZyanUSize size);

#endif

#endif

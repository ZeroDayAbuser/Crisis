
#ifndef ZYCORE_ATOMIC_H
#define ZYCORE_ATOMIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Zycore/Defines.h>
#include <Zycore/Types.h>

typedef struct ZyanAtomic32_
{
    ZyanU32 volatile value;
} ZyanAtomic32;

typedef struct ZyanAtomic64_
{
    ZyanU64 volatile value;
} ZyanAtomic64;

typedef struct ZyanAtomicPointer_
{
    ZyanVoidPointer volatile value;
} ZyanAtomicPointer;

#define ZYAN_ATOMIC_COMPARE_EXCHANGE(destination, comparand, value) \
    ZyanAtomicCompareExchange((ZyanAtomicPointer*)&(destination), (comparand), (value))

#define ZYAN_ATOMIC_INCREMENT(destination) \
    ZyanAtomicIncrement((ZyanAtomicPointer*)&(destination));

#define ZYAN_ATOMIC_DECREMENT(destination) \
    ZyanAtomicDecrement((ZyanAtomicPointer*)&(destination));

#define ZYAN_ATOMIC_COMPARE_EXCHANGE32(destination, comparand, value) \
    ZyanAtomicCompareExchange32((ZyanAtomic32*)&(destination), (comparand), (value))

#define ZYAN_ATOMIC_INCREMENT32(destination) \
    ZyanAtomicIncrement32((ZyanAtomic32*)&(destination));

#define ZYAN_ATOMIC_DECREMENT32(destination) \
    ZyanAtomicDecrement32((ZyanAtomic32*)&(destination));

#define ZYAN_ATOMIC_COMPARE_EXCHANGE64(destination, comparand, value) \
    ZyanAtomicCompareExchange64((ZyanAtomic64*)&(destination), (comparand), (value))

#define ZYAN_ATOMIC_INCREMENT64(destination) \
    ZyanAtomicIncrement64((ZyanAtomic64*)&(destination));

#define ZYAN_ATOMIC_DECREMENT64(destination) \
    ZyanAtomicDecrement64((ZyanAtomic64*)&(destination));

static ZyanUPointer ZyanAtomicCompareExchange(ZyanAtomicPointer* destination,
    ZyanUPointer comparand, ZyanUPointer value);

static ZyanUPointer ZyanAtomicIncrement(ZyanAtomicPointer* destination);

static ZyanUPointer ZyanAtomicDecrement(ZyanAtomicPointer* destination);

static ZyanU32 ZyanAtomicCompareExchange32(ZyanAtomic32* destination,
    ZyanU32 comparand, ZyanU32 value);

static ZyanU32 ZyanAtomicIncrement32(ZyanAtomic32* destination);

static ZyanU32 ZyanAtomicDecrement32(ZyanAtomic32* destination);

static ZyanU64 ZyanAtomicCompareExchange64(ZyanAtomic64* destination,
    ZyanU64 comparand, ZyanU64 value);

static ZyanU64 ZyanAtomicIncrement64(ZyanAtomic64* destination);

static ZyanU64 ZyanAtomicDecrement64(ZyanAtomic64* destination);

#if defined(ZYAN_CLANG) || defined(ZYAN_GCC) || defined(ZYAN_ICC)
#   include <Zycore/Internal/AtomicGNU.h>
#elif defined(ZYAN_MSVC)
#   include <Zycore/Internal/AtomicMSVC.h>
#else
#   error "Unsupported compiler detected"
#endif

#ifdef __cplusplus
}
#endif

#endif

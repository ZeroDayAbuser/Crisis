
#ifndef ZYCORE_ATOMIC_MSVC_H
#define ZYCORE_ATOMIC_MSVC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Windows.h>

#include <Zycore/Defines.h>
#include <Zycore/Types.h>

#if defined(ZYAN_MSVC)

#if defined(ZYAN_X86)

ZYAN_INLINE ZyanUPointer ZyanAtomicCompareExchange(ZyanAtomicPointer* destination,
    ZyanUPointer comparand, ZyanUPointer value)
{
    return (ZyanUPointer)ZyanAtomicCompareExchange32((ZyanAtomic32*)destination, comparand, value);
}

ZYAN_INLINE ZyanUPointer ZyanAtomicIncrement(ZyanAtomicPointer* destination)
{
    return (ZyanUPointer)ZyanAtomicIncrement32((ZyanAtomic32*)destination);
}

ZYAN_INLINE ZyanUPointer ZyanAtomicDecrement(ZyanAtomicPointer* destination)
{
    return (ZyanUPointer)ZyanAtomicDecrement32((ZyanAtomic32*)destination);
}

#elif defined(ZYAN_X64)

ZYAN_INLINE ZyanUPointer ZyanAtomicCompareExchange(ZyanAtomicPointer* destination,
    ZyanUPointer comparand, ZyanUPointer value)
{
    return (ZyanUPointer)ZyanAtomicCompareExchange64((ZyanAtomic64*)destination, comparand, value);
}

ZYAN_INLINE ZyanUPointer ZyanAtomicIncrement(ZyanAtomicPointer* destination)
{
    return (ZyanUPointer)ZyanAtomicIncrement64((ZyanAtomic64*)destination);
}

ZYAN_INLINE ZyanUPointer ZyanAtomicDecrement(ZyanAtomicPointer* destination)
{
    return (ZyanUPointer)ZyanAtomicDecrement64((ZyanAtomic64*)destination);
}

#else
#   error "Unsupported architecture detected"
#endif

ZYAN_INLINE ZyanU32 ZyanAtomicCompareExchange32(ZyanAtomic32* destination,
    ZyanU32 comparand, ZyanU32 value)
{
    return (ZyanU32)(_InterlockedCompareExchange((volatile LONG*)&(destination->value),
        (LONG)value, (LONG)comparand));
}

ZYAN_INLINE ZyanU32 ZyanAtomicIncrement32(ZyanAtomic32* destination)
{
    return (ZyanU32)(_InterlockedIncrement((volatile LONG*)&(destination->value)));
}

ZYAN_INLINE ZyanU32 ZyanAtomicDecrement32(ZyanAtomic32* destination)
{
    return (ZyanU32)(_InterlockedDecrement((volatile LONG*)&(destination->value)));
}

ZYAN_INLINE ZyanU64 ZyanAtomicCompareExchange64(ZyanAtomic64* destination,
    ZyanU64 comparand, ZyanU64 value)
{
    return (ZyanU64)(_InterlockedCompareExchange64((volatile LONG64*)&(destination->value),
        (LONG64)value, (LONG64)comparand));
}

ZYAN_INLINE ZyanU64 ZyanAtomicIncrement64(ZyanAtomic64* destination)
{
    return (ZyanU64)(_InterlockedIncrement64((volatile LONG64*)&(destination->value)));
}

ZYAN_INLINE ZyanU64 ZyanAtomicDecrement64(ZyanAtomic64* destination)
{
    return (ZyanU64)(_InterlockedDecrement64((volatile LONG64*)&(destination->value)));
}

#endif

#ifdef __cplusplus
}
#endif

#endif

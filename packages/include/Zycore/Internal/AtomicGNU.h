
#ifndef ZYCORE_ATOMIC_GNU_H
#define ZYCORE_ATOMIC_GNU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Zycore/Defines.h>
#include <Zycore/Types.h>

#if defined(ZYAN_CLANG) || defined(ZYAN_GCC) || defined(ZYAN_ICC)

ZYAN_INLINE ZyanUPointer ZyanAtomicCompareExchange(ZyanAtomicPointer* destination,
    ZyanUPointer comparand, ZyanUPointer value)
{
    return (ZyanUPointer)(__sync_val_compare_and_swap(
        &destination->value, (void*)comparand, (void*)value, &destination->value));
}

ZYAN_INLINE ZyanUPointer ZyanAtomicIncrement(ZyanAtomicPointer* destination)
{
    return (ZyanUPointer)(__sync_fetch_and_add(&destination->value, (void*)1,
        &destination->value)) + 1;
}

ZYAN_INLINE ZyanUPointer ZyanAtomicDecrement(ZyanAtomicPointer* destination)
{
    return (ZyanUPointer)(__sync_sub_and_fetch(&destination->value, (void*)1, &destination->value));
}

ZYAN_INLINE ZyanU32 ZyanAtomicCompareExchange32(ZyanAtomic32* destination,
    ZyanU32 comparand, ZyanU32 value)
{
    return (ZyanU32)(__sync_val_compare_and_swap(&destination->value, comparand, value,
        &destination->value));
}

ZYAN_INLINE ZyanU32 ZyanAtomicIncrement32(ZyanAtomic32* destination)
{
    return (ZyanU32)(__sync_fetch_and_add(&destination->value, 1, &destination->value)) + 1;
}

ZYAN_INLINE ZyanU32 ZyanAtomicDecrement32(ZyanAtomic32* destination)
{
    return (ZyanU32)(__sync_sub_and_fetch(&destination->value, 1, &destination->value));
}

ZYAN_INLINE ZyanU64 ZyanAtomicCompareExchange64(ZyanAtomic64* destination,
    ZyanU64 comparand, ZyanU64 value)
{
    return (ZyanU64)(__sync_val_compare_and_swap(&destination->value, comparand, value,
        &destination->value));
}

ZYAN_INLINE ZyanU64 ZyanAtomicIncrement64(ZyanAtomic64* destination)
{
    return (ZyanU64)(__sync_fetch_and_add(&destination->value, 1, &destination->value)) + 1;
}

ZYAN_INLINE ZyanU64 ZyanAtomicDecrement64(ZyanAtomic64* destination)
{
    return (ZyanU64)(__sync_sub_and_fetch(&destination->value, 1, &destination->value));
}

#endif

#ifdef __cplusplus
}
#endif

#endif

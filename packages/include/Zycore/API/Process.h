
#ifndef ZYCORE_API_PROCESS_H
#define ZYCORE_API_PROCESS_H

#include <Zycore/Status.h>
#include <Zycore/Types.h>

#ifndef ZYAN_NO_LIBC



ZYCORE_EXPORT ZyanStatus ZyanProcessFlushInstructionCache(void* address, ZyanUSize size);

#endif

#endif

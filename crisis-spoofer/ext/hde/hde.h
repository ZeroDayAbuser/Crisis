#pragma once








#define HDE_BUFFER_READ_SIZE_MAX    26

#if defined(_WIN64)
#include "hde64.h"

typedef hde64s HDE_DISASSEMBLY;

#define HdeDisassemble  hde64_disasm
#else
#include "hde32.h"

typedef hde32s HDE_DISASSEMBLY;

#define HdeDisassemble  hde32_disasm
#endif

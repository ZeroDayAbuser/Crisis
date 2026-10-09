






























#ifndef ZYDIS_UTILS_H
#define ZYDIS_UTILS_H

#include <Zycore/Defines.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

































ZYDIS_EXPORT ZyanStatus ZydisCalcAbsoluteAddress(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operand, ZyanU64 runtime_address, ZyanU64* result_address);


















ZYDIS_EXPORT ZyanStatus ZydisCalcAbsoluteAddressEx(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operand, ZyanU64 runtime_address,
    const ZydisRegisterContext* register_context, ZyanU64* result_address);









#ifdef __cplusplus
}
#endif

#endif 

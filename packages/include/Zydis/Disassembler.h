






























#ifndef ZYDIS_DISASSEMBLER_H
#define ZYDIS_DISASSEMBLER_H

#include <Zydis/Decoder.h>
#include <Zydis/Formatter.h>

#ifdef __cplusplus
extern "C" {
#endif










typedef struct ZydisDisassembledInstruction_
{
    


    ZyanU64 runtime_address;
    


    ZydisDecodedInstruction info;
    








    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    




    char text[96];
} ZydisDisassembledInstruction;






































ZYDIS_EXPORT ZyanStatus ZydisDisassembleIntel(ZydisMachineMode machine_mode,
    ZyanU64 runtime_address, const void* buffer, ZyanUSize length,
    ZydisDisassembledInstruction *instruction);






ZYDIS_EXPORT ZyanStatus ZydisDisassembleATT(ZydisMachineMode machine_mode,
    ZyanU64 runtime_address, const void* buffer, ZyanUSize length,
    ZydisDisassembledInstruction *instruction);



#ifdef __cplusplus
}
#endif

#endif 

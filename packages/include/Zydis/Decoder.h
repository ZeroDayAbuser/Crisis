






























#ifndef ZYDIS_DECODER_H
#define ZYDIS_DECODER_H

#include <Zycore/Types.h>
#include <Zycore/Defines.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif












typedef enum ZydisDecoderMode_
{
    











    ZYDIS_DECODER_MODE_MINIMAL,
    









    ZYDIS_DECODER_MODE_AMD_BRANCHES,
    












    ZYDIS_DECODER_MODE_KNC,
    






    ZYDIS_DECODER_MODE_MPX,
    






    ZYDIS_DECODER_MODE_CET,
    






    ZYDIS_DECODER_MODE_LZCNT,
    






    ZYDIS_DECODER_MODE_TZCNT,
    







    ZYDIS_DECODER_MODE_WBNOINVD,
     






    ZYDIS_DECODER_MODE_CLDEMOTE,
    






    ZYDIS_DECODER_MODE_IPREFETCH,
    







    ZYDIS_DECODER_MODE_UD0_COMPAT,
    






    ZYDIS_DECODER_MODE_APX,

    


    ZYDIS_DECODER_MODE_MAX_VALUE = ZYDIS_DECODER_MODE_APX,
    


    ZYDIS_DECODER_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_DECODER_MODE_MAX_VALUE)
} ZydisDecoderMode;











typedef struct ZydisDecoder_
{
    


    ZydisMachineMode machine_mode;
    


    ZydisStackWidth stack_width;
    


    ZyanU32 decoder_mode;
} ZydisDecoder;






















ZYDIS_EXPORT ZyanStatus ZydisDecoderInit(ZydisDecoder* decoder, ZydisMachineMode machine_mode,
    ZydisStackWidth stack_width);










ZYDIS_EXPORT ZyanStatus ZydisDecoderEnableMode(ZydisDecoder* decoder, ZydisDecoderMode mode,
    ZyanBool enabled);





























ZYDIS_EXPORT ZyanStatus ZydisDecoderDecodeFull(const ZydisDecoder* decoder,
    const void* buffer, ZyanUSize length, ZydisDecodedInstruction* instruction,
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]);


















ZYDIS_EXPORT ZyanStatus ZydisDecoderDecodeInstruction(const ZydisDecoder* decoder,
    ZydisDecoderContext* context, const void* buffer, ZyanUSize length,
    ZydisDecodedInstruction* instruction);



























ZYDIS_EXPORT ZyanStatus ZydisDecoderDecodeOperands(const ZydisDecoder* decoder,
    const ZydisDecoderContext* context, const ZydisDecodedInstruction* instruction,
    ZydisDecodedOperand* operands, ZyanU8 operand_count);





#ifdef __cplusplus
}
#endif

#endif 

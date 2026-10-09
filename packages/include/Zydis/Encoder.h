






























#ifndef ZYDIS_ENCODER_H
#define ZYDIS_ENCODER_H

#include <Zycore/Types.h>
#include <Zydis/MetaInfo.h>
#include <Zydis/Register.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Mnemonic.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif












#define ZYDIS_ENCODER_MAX_OPERANDS 5


ZYAN_STATIC_ASSERT(ZYAN_BITS_TO_REPRESENT(ZYDIS_ENCODER_MAX_OPERANDS) == 3);




#define ZYDIS_ENCODABLE_PREFIXES   (ZYDIS_ATTRIB_HAS_LOCK | \
                                    ZYDIS_ATTRIB_HAS_REP | \
                                    ZYDIS_ATTRIB_HAS_REPE | \
                                    ZYDIS_ATTRIB_HAS_REPNE | \
                                    ZYDIS_ATTRIB_HAS_BND | \
                                    ZYDIS_ATTRIB_HAS_XACQUIRE | \
                                    ZYDIS_ATTRIB_HAS_XRELEASE | \
                                    ZYDIS_ATTRIB_HAS_BRANCH_NOT_TAKEN | \
                                    ZYDIS_ATTRIB_HAS_BRANCH_TAKEN | \
                                    ZYDIS_ATTRIB_HAS_NOTRACK | \
                                    ZYDIS_ATTRIB_HAS_SEGMENT_CS | \
                                    ZYDIS_ATTRIB_HAS_SEGMENT_SS | \
                                    ZYDIS_ATTRIB_HAS_SEGMENT_DS | \
                                    ZYDIS_ATTRIB_HAS_SEGMENT_ES | \
                                    ZYDIS_ATTRIB_HAS_SEGMENT_FS | \
                                    ZYDIS_ATTRIB_HAS_SEGMENT_GS)











typedef enum ZydisEncodableEncoding_
{
    ZYDIS_ENCODABLE_ENCODING_DEFAULT                = 0x00000000,
    ZYDIS_ENCODABLE_ENCODING_LEGACY                 = 0x00000001,
    ZYDIS_ENCODABLE_ENCODING_3DNOW                  = 0x00000002,
    ZYDIS_ENCODABLE_ENCODING_XOP                    = 0x00000004,
    ZYDIS_ENCODABLE_ENCODING_VEX                    = 0x00000008,
    ZYDIS_ENCODABLE_ENCODING_EVEX                   = 0x00000010,
    ZYDIS_ENCODABLE_ENCODING_MVEX                   = 0x00000020,

    


    ZYDIS_ENCODABLE_ENCODING_MAX_VALUE              = (ZYDIS_ENCODABLE_ENCODING_MVEX | 
                                                       (ZYDIS_ENCODABLE_ENCODING_MVEX - 1)),
    


    ZYDIS_ENCODABLE_ENCODING_REQUIRED_BITS          = 
        ZYAN_BITS_TO_REPRESENT(ZYDIS_ENCODABLE_ENCODING_MAX_VALUE)
} ZydisEncodableEncoding;





typedef enum ZydisBranchWidth_
{
    ZYDIS_BRANCH_WIDTH_NONE,
    ZYDIS_BRANCH_WIDTH_8,
    ZYDIS_BRANCH_WIDTH_16,
    ZYDIS_BRANCH_WIDTH_32,
    ZYDIS_BRANCH_WIDTH_64,

    


    ZYDIS_BRANCH_WIDTH_MAX_VALUE = ZYDIS_BRANCH_WIDTH_64,
    


    ZYDIS_BRANCH_WIDTH_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_BRANCH_WIDTH_MAX_VALUE)
} ZydisBranchWidth;





typedef enum ZydisAddressSizeHint_
{
    ZYDIS_ADDRESS_SIZE_HINT_NONE,
    ZYDIS_ADDRESS_SIZE_HINT_16,
    ZYDIS_ADDRESS_SIZE_HINT_32,
    ZYDIS_ADDRESS_SIZE_HINT_64,

    


    ZYDIS_ADDRESS_SIZE_HINT_MAX_VALUE = ZYDIS_ADDRESS_SIZE_HINT_64,
    


    ZYDIS_ADDRESS_SIZE_HINT_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_ADDRESS_SIZE_HINT_MAX_VALUE)
} ZydisAddressSizeHint;





typedef enum ZydisOperandSizeHint_
{
    ZYDIS_OPERAND_SIZE_HINT_NONE,
    ZYDIS_OPERAND_SIZE_HINT_8,
    ZYDIS_OPERAND_SIZE_HINT_16,
    ZYDIS_OPERAND_SIZE_HINT_32,
    ZYDIS_OPERAND_SIZE_HINT_64,

    


    ZYDIS_OPERAND_SIZE_HINT_MAX_VALUE = ZYDIS_OPERAND_SIZE_HINT_64,
    


    ZYDIS_OPERAND_SIZE_HINT_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_OPERAND_SIZE_HINT_MAX_VALUE)
} ZydisOperandSizeHint;




typedef struct ZydisEncoderOperand_
{
    


    ZydisOperandType type;
    


    struct ZydisEncoderOperandReg_
    {
        


        ZydisRegister value;
        




        ZyanBool is4;
    } reg;
    


    struct ZydisEncoderOperandMem_
    {
        


        ZydisRegister base;
        


        ZydisRegister index;
        


        ZyanU8 scale;
        






        ZyanI64 displacement;
        


        ZyanU16 size;
    } mem;
    


    struct ZydisEncoderOperandPtr_
    {
        


        ZyanU16 segment;
        


        ZyanU32 offset;
    } ptr;
    


    union ZydisEncoderOperandImm_
    {
        


        ZyanU64 u;
        


        ZyanI64 s;
    } imm;
} ZydisEncoderOperand;




typedef struct ZydisEncoderRequest_
{
    


    ZydisMachineMode machine_mode;
    





    ZydisEncodableEncoding allowed_encodings;
    


    ZydisMnemonic mnemonic;
    



    ZydisInstructionAttributes prefixes;
    




    ZydisBranchType branch_type;
    





    ZydisBranchWidth branch_width;
    












    ZydisAddressSizeHint address_size_hint;
    





    ZydisOperandSizeHint operand_size_hint;
    




    ZyanU8 operand_count;
    


    ZydisEncoderOperand operands[ZYDIS_ENCODER_MAX_OPERANDS];
    


    struct ZydisEncoderRequestEvexFeatures_
    {
        



        ZydisBroadcastMode broadcast;
        


        ZydisRoundingMode rounding;
        



        ZyanBool sae;
        



        ZyanBool zeroing_mask;
        


        ZyanBool no_flags;
        


        ZydisDefaultFlagsValue default_flags;
    } evex;
    


    struct ZydisEncoderRequestMvexFeatures_
    {
        


        ZydisBroadcastMode broadcast;
        


        ZydisConversionMode conversion;
        


        ZydisRoundingMode rounding;
        


        ZydisSwizzleMode swizzle;
        



        ZyanBool sae;
        


        ZyanBool eviction_hint;
    } mvex;
} ZydisEncoderRequest;





















ZYDIS_EXPORT ZyanStatus ZydisEncoderEncodeInstruction(const ZydisEncoderRequest *request, 
    void *buffer, ZyanUSize *length);
















ZYDIS_EXPORT ZyanStatus ZydisEncoderEncodeInstructionAbsolute(ZydisEncoderRequest *request,
    void *buffer, ZyanUSize *length, ZyanU64 runtime_address);


















ZYDIS_EXPORT ZyanStatus ZydisEncoderDecodedInstructionToEncoderRequest(
    const ZydisDecodedInstruction* instruction, const ZydisDecodedOperand* operands,
    ZyanU8 operand_count_visible, ZydisEncoderRequest* request);









ZYDIS_EXPORT ZyanStatus ZydisEncoderNopFill(void *buffer, ZyanUSize length);





#ifdef __cplusplus
}
#endif

#endif 

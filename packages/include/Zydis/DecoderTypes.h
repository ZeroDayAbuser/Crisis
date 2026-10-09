






























#ifndef ZYDIS_INSTRUCTIONINFO_H
#define ZYDIS_INSTRUCTIONINFO_H

#include <Zycore/Types.h>
#include <Zydis/MetaInfo.h>
#include <Zydis/Mnemonic.h>
#include <Zydis/Register.h>
#include <Zydis/SharedTypes.h>

#ifdef __cplusplus
extern "C" {
#endif












typedef ZyanU8 ZydisOperandAttributes;









#define ZYDIS_OATTRIB_IS_MULTISOURCE4   (1 <<  0)








typedef enum ZydisMemoryOperandType_
{
    ZYDIS_MEMOP_TYPE_INVALID,
    


    ZYDIS_MEMOP_TYPE_MEM,
    



    ZYDIS_MEMOP_TYPE_AGEN,
    



    ZYDIS_MEMOP_TYPE_MIB,
    


    ZYDIS_MEMOP_TYPE_VSIB,

    


    ZYDIS_MEMOP_TYPE_MAX_VALUE = ZYDIS_MEMOP_TYPE_VSIB,
    


    ZYDIS_MEMOP_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_MEMOP_TYPE_MAX_VALUE)
} ZydisMemoryOperandType;








typedef struct ZydisDecodedOperandReg_
{
    


    ZydisRegister value;
} ZydisDecodedOperandReg;




typedef struct ZydisDecodedOperandMem_
{
    


    ZydisMemoryOperandType type;
    


    ZydisRegister segment;
    


    ZydisRegister base;
    


    ZydisRegister index;
    


    ZyanU8 scale;
    


    struct ZydisDecodedOperandMemDisp_
    {
        


        ZyanI64 value;
        



        ZyanU8 offset;
        


        ZyanU8 size;
    } disp;
} ZydisDecodedOperandMem;




typedef struct ZydisDecodedOperandPtr_
{
    ZyanU16 segment;
    ZyanU32 offset;
} ZydisDecodedOperandPtr;




typedef struct ZydisDecodedOperandImm_
{
    


    ZyanBool is_signed;
    


    ZyanBool is_address;
    



    ZyanBool is_relative;
    


    union ZydisDecodedOperandImmValue_
    {
        ZyanU64 u;
        ZyanI64 s;
    } value;
    



    ZyanU8 offset;
    


    ZyanU8 size;
} ZydisDecodedOperandImm;




typedef struct ZydisDecodedOperand_
{
    


    ZyanU8 id;
    


    ZydisOperandVisibility visibility;
    


    ZydisOperandActions actions;
    


    ZydisOperandEncoding encoding;
    


    ZyanU16 size;
    


    ZydisElementType element_type;
    


    ZydisElementSize element_size;
    


    ZyanU16 element_count;
    


    ZydisOperandAttributes attributes;
    


    ZydisOperandType type;
    




    union
    {
        ZydisDecodedOperandReg reg;
        ZydisDecodedOperandMem mem;
        ZydisDecodedOperandPtr ptr;
        ZydisDecodedOperandImm imm;
    };
} ZydisDecodedOperand;














typedef ZyanU32 ZydisAccessedFlagsMask;













#define ZYDIS_CPUFLAG_CF    (1ul <<  0)



#define ZYDIS_CPUFLAG_PF    (1ul <<  2)



#define ZYDIS_CPUFLAG_AF    (1ul <<  4)



#define ZYDIS_CPUFLAG_ZF    (1ul <<  6)



#define ZYDIS_CPUFLAG_SF    (1ul <<  7)



#define ZYDIS_CPUFLAG_TF    (1ul <<  8)



#define ZYDIS_CPUFLAG_IF    (1ul <<  9)



#define ZYDIS_CPUFLAG_DF    (1ul << 10)



#define ZYDIS_CPUFLAG_OF    (1ul << 11)



#define ZYDIS_CPUFLAG_IOPL  (1ul << 12)



#define ZYDIS_CPUFLAG_NT    (1ul << 14)



#define ZYDIS_CPUFLAG_RF    (1ul << 16)



#define ZYDIS_CPUFLAG_VM    (1ul << 17)



#define ZYDIS_CPUFLAG_AC    (1ul << 18)



#define ZYDIS_CPUFLAG_VIF   (1ul << 19)



#define ZYDIS_CPUFLAG_VIP   (1ul << 20)



#define ZYDIS_CPUFLAG_ID    (1ul << 21)

















#define ZYDIS_FPUFLAG_C0    (1ul <<  0)



#define ZYDIS_FPUFLAG_C1    (1ul <<  1)
 


#define ZYDIS_FPUFLAG_C2    (1ul <<  2)



#define ZYDIS_FPUFLAG_C3    (1ul <<  3)








typedef struct ZydisAccessedFlags_
{
    


    ZydisAccessedFlagsMask tested;
    


    ZydisAccessedFlagsMask modified;
    


    ZydisAccessedFlagsMask set_0;
    


    ZydisAccessedFlagsMask set_1;
    


    ZydisAccessedFlagsMask undefined;
} ZydisAccessedFlags;








typedef enum ZydisBranchType_
{
    


    ZYDIS_BRANCH_TYPE_NONE,
    


    ZYDIS_BRANCH_TYPE_SHORT,
    


    ZYDIS_BRANCH_TYPE_NEAR,
    


    ZYDIS_BRANCH_TYPE_FAR,
    


    ZYDIS_BRANCH_TYPE_ABSOLUTE,

    


    ZYDIS_BRANCH_TYPE_MAX_VALUE = ZYDIS_BRANCH_TYPE_ABSOLUTE,
    


    ZYDIS_BRANCH_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_BRANCH_TYPE_MAX_VALUE)
} ZydisBranchType;








typedef enum ZydisExceptionClass_
{
    ZYDIS_EXCEPTION_CLASS_NONE,
    
    ZYDIS_EXCEPTION_CLASS_SSE1,
    ZYDIS_EXCEPTION_CLASS_SSE2,
    ZYDIS_EXCEPTION_CLASS_SSE3,
    ZYDIS_EXCEPTION_CLASS_SSE4,
    ZYDIS_EXCEPTION_CLASS_SSE5,
    ZYDIS_EXCEPTION_CLASS_SSE7,
    ZYDIS_EXCEPTION_CLASS_AVX1,
    ZYDIS_EXCEPTION_CLASS_AVX2,
    ZYDIS_EXCEPTION_CLASS_AVX3,
    ZYDIS_EXCEPTION_CLASS_AVX4,
    ZYDIS_EXCEPTION_CLASS_AVX5,
    ZYDIS_EXCEPTION_CLASS_AVX6,
    ZYDIS_EXCEPTION_CLASS_AVX7,
    ZYDIS_EXCEPTION_CLASS_AVX8,
    ZYDIS_EXCEPTION_CLASS_AVX11,
    ZYDIS_EXCEPTION_CLASS_AVX12,
    ZYDIS_EXCEPTION_CLASS_AVX14,
    ZYDIS_EXCEPTION_CLASS_E1,
    ZYDIS_EXCEPTION_CLASS_E1NF,
    ZYDIS_EXCEPTION_CLASS_E2,
    ZYDIS_EXCEPTION_CLASS_E2NF,
    ZYDIS_EXCEPTION_CLASS_E3,
    ZYDIS_EXCEPTION_CLASS_E3NF,
    ZYDIS_EXCEPTION_CLASS_E4,
    ZYDIS_EXCEPTION_CLASS_E4NF,
    ZYDIS_EXCEPTION_CLASS_E5,
    ZYDIS_EXCEPTION_CLASS_E5NF,
    ZYDIS_EXCEPTION_CLASS_E6,
    ZYDIS_EXCEPTION_CLASS_E6NF,
    ZYDIS_EXCEPTION_CLASS_E7NM,
    ZYDIS_EXCEPTION_CLASS_E7NM128,
    ZYDIS_EXCEPTION_CLASS_E9NF,
    ZYDIS_EXCEPTION_CLASS_E10,
    ZYDIS_EXCEPTION_CLASS_E10NF,
    ZYDIS_EXCEPTION_CLASS_E11,
    ZYDIS_EXCEPTION_CLASS_E11NF,
    ZYDIS_EXCEPTION_CLASS_E12,
    ZYDIS_EXCEPTION_CLASS_E12NP,
    ZYDIS_EXCEPTION_CLASS_K20,
    ZYDIS_EXCEPTION_CLASS_K21,
    ZYDIS_EXCEPTION_CLASS_AMXE1,
    ZYDIS_EXCEPTION_CLASS_AMXE2,
    ZYDIS_EXCEPTION_CLASS_AMXE3,
    ZYDIS_EXCEPTION_CLASS_AMXE4,
    ZYDIS_EXCEPTION_CLASS_AMXE5,
    ZYDIS_EXCEPTION_CLASS_AMXE6,
    ZYDIS_EXCEPTION_CLASS_AMXE1_EVEX,
    ZYDIS_EXCEPTION_CLASS_AMXE2_EVEX,
    ZYDIS_EXCEPTION_CLASS_AMXE3_EVEX,
    ZYDIS_EXCEPTION_CLASS_AMXE4_EVEX,
    ZYDIS_EXCEPTION_CLASS_AMXE5_EVEX,
    ZYDIS_EXCEPTION_CLASS_AMXE6_EVEX,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_INT,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_KEYLOCKER,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_BMI,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_CCMP,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_CFCMOV,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_CMPCCXADD,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_ENQCMD,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_INVEPT,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_INVPCID,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_INVVPID,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_KMOV,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_PP2,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_SHA,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_CET_WRSS,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_CET_WRUSS,
    ZYDIS_EXCEPTION_CLASS_APX_LEGACY_JMPABS,
    ZYDIS_EXCEPTION_CLASS_APX_EVEX_RAO_INT,
    ZYDIS_EXCEPTION_CLASS_USER_MSR_EVEX,
    ZYDIS_EXCEPTION_CLASS_LEGACY_RAO_INT,

    


    ZYDIS_EXCEPTION_CLASS_MAX_VALUE = ZYDIS_EXCEPTION_CLASS_LEGACY_RAO_INT,
    


    ZYDIS_EXCEPTION_CLASS_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_EXCEPTION_CLASS_MAX_VALUE)
} ZydisExceptionClass;








typedef enum ZydisMaskMode_
{
    ZYDIS_MASK_MODE_NONE,
    


    ZYDIS_MASK_MODE_DISABLED,
    


    ZYDIS_MASK_MODE_MERGING,
    


    ZYDIS_MASK_MODE_ZEROING,
    


    ZYDIS_MASK_MODE_CONTROL,
    


    ZYDIS_MASK_MODE_CONTROL_ZEROING,

    


    ZYDIS_MASK_MODE_MAX_VALUE = ZYDIS_MASK_MODE_CONTROL_ZEROING,
    


    ZYDIS_MASK_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_MASK_MODE_MAX_VALUE)
} ZydisMaskMode;








typedef enum ZydisBroadcastMode_
{
    ZYDIS_BROADCAST_MODE_NONE,
    ZYDIS_BROADCAST_MODE_1_TO_2,
    ZYDIS_BROADCAST_MODE_1_TO_4,
    ZYDIS_BROADCAST_MODE_1_TO_8,
    ZYDIS_BROADCAST_MODE_1_TO_16,
    ZYDIS_BROADCAST_MODE_1_TO_32,
    ZYDIS_BROADCAST_MODE_1_TO_64,
    ZYDIS_BROADCAST_MODE_2_TO_4,
    ZYDIS_BROADCAST_MODE_2_TO_8,
    ZYDIS_BROADCAST_MODE_2_TO_16,
    ZYDIS_BROADCAST_MODE_4_TO_8,
    ZYDIS_BROADCAST_MODE_4_TO_16,
    ZYDIS_BROADCAST_MODE_8_TO_16,

    


    ZYDIS_BROADCAST_MODE_MAX_VALUE = ZYDIS_BROADCAST_MODE_8_TO_16,
    


    ZYDIS_BROADCAST_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_BROADCAST_MODE_MAX_VALUE)
} ZydisBroadcastMode;








typedef enum ZydisRoundingMode_
{
    ZYDIS_ROUNDING_MODE_NONE,
    


    ZYDIS_ROUNDING_MODE_RN,
    


    ZYDIS_ROUNDING_MODE_RD,
    


    ZYDIS_ROUNDING_MODE_RU,
    


    ZYDIS_ROUNDING_MODE_RZ,

    


    ZYDIS_ROUNDING_MODE_MAX_VALUE = ZYDIS_ROUNDING_MODE_RZ,
    


    ZYDIS_ROUNDING_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_ROUNDING_MODE_MAX_VALUE)
} ZydisRoundingMode;








typedef enum ZydisSwizzleMode_
{
    ZYDIS_SWIZZLE_MODE_NONE,
    ZYDIS_SWIZZLE_MODE_DCBA,
    ZYDIS_SWIZZLE_MODE_CDAB,
    ZYDIS_SWIZZLE_MODE_BADC,
    ZYDIS_SWIZZLE_MODE_DACB,
    ZYDIS_SWIZZLE_MODE_AAAA,
    ZYDIS_SWIZZLE_MODE_BBBB,
    ZYDIS_SWIZZLE_MODE_CCCC,
    ZYDIS_SWIZZLE_MODE_DDDD,

    


    ZYDIS_SWIZZLE_MODE_MAX_VALUE = ZYDIS_SWIZZLE_MODE_DDDD,
    


    ZYDIS_SWIZZLE_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_SWIZZLE_MODE_MAX_VALUE)
} ZydisSwizzleMode;








typedef enum ZydisConversionMode_
{
    ZYDIS_CONVERSION_MODE_NONE,
    ZYDIS_CONVERSION_MODE_FLOAT16,
    ZYDIS_CONVERSION_MODE_SINT8,
    ZYDIS_CONVERSION_MODE_UINT8,
    ZYDIS_CONVERSION_MODE_SINT16,
    ZYDIS_CONVERSION_MODE_UINT16,

    


    ZYDIS_CONVERSION_MODE_MAX_VALUE = ZYDIS_CONVERSION_MODE_UINT16,
    


    ZYDIS_CONVERSION_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_CONVERSION_MODE_MAX_VALUE)
} ZydisConversionMode;








typedef ZyanU8 ZydisDefaultFlagsValue;














#define ZYDIS_DFV_CF    (1u << 0)



#define ZYDIS_DFV_ZF    (1u << 1)



#define ZYDIS_DFV_SF    (1u << 2)



#define ZYDIS_DFV_OF    (1u << 3)




#define ZYDIS_DFV_NONE  0



#define ZYDIS_DFV_ALL   (ZYDIS_DFV_CF | ZYDIS_DFV_ZF | ZYDIS_DFV_SF | ZYDIS_DFV_OF)












typedef enum ZydisSourceConditionCode_
{
    ZYDIS_SCC_NONE,
    ZYDIS_SCC_O,
    ZYDIS_SCC_NO,
    ZYDIS_SCC_B,
    ZYDIS_SCC_NB,
    ZYDIS_SCC_Z,
    ZYDIS_SCC_NZ,
    ZYDIS_SCC_BE,
    ZYDIS_SCC_NBE,
    ZYDIS_SCC_S,
    ZYDIS_SCC_NS,
    ZYDIS_SCC_TRUE,
    ZYDIS_SCC_FALSE,
    ZYDIS_SCC_L,
    ZYDIS_SCC_NL,
    ZYDIS_SCC_LE,
    ZYDIS_SCC_NLE,

    


    ZYDIS_SCC_MAX_VALUE = ZYDIS_SCC_NLE,
    


    ZYDIS_SCC_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_SCC_MAX_VALUE)
} ZydisSourceConditionCode;








typedef enum ZydisPrefixType_
{
    





    ZYDIS_PREFIX_TYPE_IGNORED,
    


    ZYDIS_PREFIX_TYPE_EFFECTIVE,
    





    ZYDIS_PREFIX_TYPE_MANDATORY,

    


    ZYDIS_PREFIX_TYPE_MAX_VALUE = ZYDIS_PREFIX_TYPE_MANDATORY,
    


    ZYDIS_PREFIX_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_PREFIX_TYPE_MAX_VALUE)
} ZydisPrefixType;










typedef struct ZydisDecodedInstructionRawRex_
{
    


    ZyanU8 W;
    


    ZyanU8 R;
    


    ZyanU8 X;
    


    ZyanU8 B;
    










    ZyanU8 offset;
} ZydisDecodedInstructionRawRex;




typedef struct ZydisDecodedInstructionRawRex2_
{
    


    ZyanU8 M0;
    


    ZyanU8 R4;
    


    ZyanU8 X4;
    


    ZyanU8 B4;
    


    ZyanU8 W;
    


    ZyanU8 R3;
    


    ZyanU8 X3;
    


    ZyanU8 B3;
    







    ZyanU8 offset;
} ZydisDecodedInstructionRawRex2;




typedef struct ZydisDecodedInstructionRawXop_
{
    


    ZyanU8 R;
    


    ZyanU8 X;
    


    ZyanU8 B;
    


    ZyanU8 m_mmmm;
    


    ZyanU8 W;
    



    ZyanU8 vvvv;
    


    ZyanU8 L;
    


    ZyanU8 pp;
    



    ZyanU8 offset;
} ZydisDecodedInstructionRawXop;




typedef struct ZydisDecodedInstructionRawVex_
{
    


    ZyanU8 R;
    


    ZyanU8 X;
    


    ZyanU8 B;
    


    ZyanU8 m_mmmm;
    


    ZyanU8 W;
    



    ZyanU8 vvvv;
    


    ZyanU8 L;
    


    ZyanU8 pp;
    



    ZyanU8 offset;
    


    ZyanU8 size;
} ZydisDecodedInstructionRawVex;




typedef struct ZydisDecodedInstructionRawEvex_
{
    


    ZyanU8 R3;
    


    ZyanU8 X3;
    


    ZyanU8 B3;
    


    ZyanU8 R4;
    


    ZyanU8 B4;
    


    ZyanU8 mmm;
    


    ZyanU8 W;
    



    ZyanU8 vvvv;
    


    ZyanU8 U;
    


    ZyanU8 X4;
    


    ZyanU8 pp;
    


    ZyanU8 z;
    


    ZyanU8 L2;
    


    ZyanU8 L;
    


    ZyanU8 b;
    


    ZyanU8 V4;
    


    ZyanU8 aaa;

    ZyanU8 ND;
    ZyanU8 NF;
    ZyanU8 SCC;

    



    ZyanU8 offset;
} ZydisDecodedInstructionRawEvex;




typedef struct ZydisDecodedInstructionRawMvex_
{
    


    ZyanU8 R;
    


    ZyanU8 X;
    


    ZyanU8 B;
    


    ZyanU8 R2;
    


    ZyanU8 mmmm;
    


    ZyanU8 W;
    



    ZyanU8 vvvv;
    


    ZyanU8 pp;
    


    ZyanU8 E;
    


    ZyanU8 SSS;
    


    ZyanU8 V2;
    


    ZyanU8 kkk;
    



    ZyanU8 offset;
} ZydisDecodedInstructionRawMvex;




typedef struct ZydisDecodedInstructionAvx_
{
    


    ZyanU16 vector_length;
    


    struct ZydisDecodedInstructionAvxMask_
    {
        


        ZydisMaskMode mode;
        


        ZydisRegister reg;
    } mask;
    


    struct ZydisDecodedInstructionAvxBroadcast_
    {
        





        ZyanBool is_static;
        


        ZydisBroadcastMode mode;
    } broadcast;
    


    struct ZydisDecodedInstructionAvxRounding_
    {
        


        ZydisRoundingMode mode;
    } rounding;
    


    struct ZydisDecodedInstructionAvxSwizzle_
    {
        


        ZydisSwizzleMode mode;
    } swizzle;
    


    struct ZydisDecodedInstructionAvxConversion_
    {
        


        ZydisConversionMode mode;
    } conversion;
    



    ZyanBool has_sae;
    


    ZyanBool has_eviction_hint;
    
} ZydisDecodedInstructionAvx;




typedef struct ZydisDecodedInstructionApx_
{
    


    ZyanBool uses_egpr;
    


    ZyanBool has_nf;
    


    ZyanBool has_zu;
    




    ZyanBool has_ppx;
    


    ZydisSourceConditionCode scc;
    





    ZydisDefaultFlagsValue default_flags;
} ZydisDecodedInstructionApx;




typedef struct ZydisDecodedInstructionMeta_
{
    


    ZydisInstructionCategory category;
    


    ZydisISASet isa_set;
    


    ZydisISAExt isa_ext;
    


    ZydisBranchType branch_type;
    


    ZydisExceptionClass exception_class;
} ZydisDecodedInstructionMeta;





typedef struct ZydisDecodedInstructionRaw_
{
    


    ZyanU8 prefix_count;
    


    struct ZydisDecodedInstructionRawPrefixes_
    {
        


        ZydisPrefixType type;
        


        ZyanU8 value;
    } prefixes[ZYDIS_MAX_INSTRUCTION_LENGTH];

    





    ZydisInstructionEncoding encoding2;
    


    union
    {
        ZydisDecodedInstructionRawRex rex;
        ZydisDecodedInstructionRawRex2 rex2;
        ZydisDecodedInstructionRawXop xop;
        ZydisDecodedInstructionRawVex vex;
        ZydisDecodedInstructionRawEvex evex;
        ZydisDecodedInstructionRawMvex mvex;
    };

    


    struct ZydisDecodedInstructionModRm_
    {
        


        ZyanU8 mod;
        


        ZyanU8 reg;
        


        ZyanU8 rm;
        



        ZyanU8 offset;
    } modrm;
    


    struct ZydisDecodedInstructionRawSib_
    {
        


        ZyanU8 scale;
        


        ZyanU8 index;
        


        ZyanU8 base;
        



        ZyanU8 offset;
    } sib;
    


    struct ZydisDecodedInstructionRawDisp_
    {
        


        ZyanI64 value;
        


        ZyanU8 size;
        
        



        ZyanU8 offset;
    } disp;
    


    struct ZydisDecodedInstructionRawImm_
    {
        


        ZyanBool is_signed;
        


        ZyanBool is_address;
        



        ZyanBool is_relative;
        


        union ZydisDecodedInstructionRawImmValue_
        {
            ZyanU64 u;
            ZyanI64 s;
        } value;
        


        ZyanU8 size;
        



        ZyanU8 offset;
    } imm[2];
} ZydisDecodedInstructionRaw;




typedef struct ZydisDecodedInstruction_
{
    


    ZydisMachineMode machine_mode;
    


    ZydisMnemonic mnemonic;
    


    ZyanU8 length;
    


    ZydisInstructionEncoding encoding;
    


    ZydisOpcodeMap opcode_map;
    


    ZyanU8 opcode;
    


    ZyanU8 stack_width;
    


    ZyanU8 operand_width;
    


    ZyanU8 address_width;
    






    ZyanU8 operand_count;
    





    ZyanU8 operand_count_visible;
    


    ZydisInstructionAttributes attributes;
    





    const ZydisAccessedFlags* cpu_flags;
    




    const ZydisAccessedFlags* fpu_flags;
    


    ZydisDecodedInstructionAvx avx;
    


    ZydisDecodedInstructionApx apx;
    


    ZydisDecodedInstructionMeta meta;
    



    ZydisDecodedInstructionRaw raw;
} ZydisDecodedInstruction;

















typedef struct ZydisDecoderContext_
{
    


    const void* definition;
    




    ZyanU8 eosz_index;
    




    ZyanU8 easz_index;
    


    struct
    {
        ZyanU8 W;
        ZyanU8 R3;
        ZyanU8 R4;
        ZyanU8 X3;
        ZyanU8 X4;
        ZyanU8 B3;
        ZyanU8 B4;
        ZyanU8 L;
        ZyanU8 LL;
        ZyanU8 V4;
        ZyanU8 vvvv;
        ZyanU8 mask;
    } vector_unified;
    


    struct
    {
        


        ZyanBool is_mod_reg;
        


        ZyanU8 id_reg;
        




        ZyanU8 id_rm;
        


        ZyanU8 id_ndsndd;
        




        ZyanU8 id_base;
        





        ZyanU8 id_index;
    } reg_info;
    


    struct
    {
        


        ZyanU8 tuple_type;
        


        ZyanU8 element_size;
    } evex;
    


    struct
    {
        


        ZyanU8 functionality;
    } mvex;
    


    ZyanU8 cd8_scale; 
} ZydisDecoderContext;





#ifdef __cplusplus
}
#endif

#endif 

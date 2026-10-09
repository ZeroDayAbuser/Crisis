

#ifndef ZYDIS_INTERNAL_DECODERDATA_H
#define ZYDIS_INTERNAL_DECODERDATA_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>
#include <Zydis/Defines.h>

#ifdef __cplusplus
extern "C" {
#endif






#ifdef ZYAN_MSVC
#   pragma warning(push)
#   pragma warning(disable:4214)
#endif

#pragma pack(push, 1)






typedef ZyanU16 ZydisDecoderTreeNode;




typedef enum ZydisDecoderTreeNodeType_
{
    ZYDIS_NODETYPE_INVALID = 0,
    
    ZYDIS_NODETYPE_DEFINITION,
    
    ZYDIS_NODETYPE_SWITCH_TABLE,
    
    ZYDIS_NODETYPE_SWITCH_TABLE_XOP,
    
    ZYDIS_NODETYPE_SWITCH_TABLE_VEX,
    
    ZYDIS_NODETYPE_SWITCH_TABLE_EMVEX,
    
    ZYDIS_NODETYPE_SWITCH_TABLE_REX2,
    
    ZYDIS_NODETYPE_OPCODE_TABLE,
    
    ZYDIS_NODETYPE_MODE,
    
    ZYDIS_NODETYPE_MODE_COMPACT,
    
    ZYDIS_NODETYPE_MODRM_MOD,
    
    ZYDIS_NODETYPE_MODRM_MOD_COMPACT,
    
    ZYDIS_NODETYPE_MODRM_REG,
    
    ZYDIS_NODETYPE_MODRM_RM,
    
    ZYDIS_NODETYPE_PREFIX_GROUP1,
    
    ZYDIS_NODETYPE_MANDATORY_PREFIX,
    
    ZYDIS_NODETYPE_OPERAND_SIZE,
    
    ZYDIS_NODETYPE_ADDRESS_SIZE,
    
    ZYDIS_NODETYPE_VECTOR_LENGTH,
    
    ZYDIS_NODETYPE_REX_W,
    
    ZYDIS_NODETYPE_REX_B,
    
    ZYDIS_NODETYPE_EVEX_B,
    
    ZYDIS_NODETYPE_MVEX_E,
    
    ZYDIS_NODETYPE_MODE_AMD,
    
    ZYDIS_NODETYPE_MODE_KNC,
    
    ZYDIS_NODETYPE_MODE_MPX,
    
    ZYDIS_NODETYPE_MODE_CET,
    
    ZYDIS_NODETYPE_MODE_LZCNT,
    
    ZYDIS_NODETYPE_MODE_TZCNT,
    
    ZYDIS_NODETYPE_MODE_WBNOINVD,
    
    ZYDIS_NODETYPE_MODE_CLDEMOTE,
    
    ZYDIS_NODETYPE_MODE_IPREFETCH,
    
    ZYDIS_NODETYPE_MODE_UD0_COMPAT,
    
    ZYDIS_NODETYPE_EVEX_ND,
    
    ZYDIS_NODETYPE_EVEX_NF,
    
    ZYDIS_NODETYPE_EVEX_SCC,
    
    ZYDIS_NODETYPE_REX_2,
    
    ZYDIS_NODETYPE_EVEX_U,

    
    ZYDIS_NODETYPE_MAX_VALUE = ZYDIS_NODETYPE_EVEX_U,
    
    ZYDIS_NODETYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_NODETYPE_MAX_VALUE)
} ZydisDecoderTreeNodeType;







#define ZYDIS_OPCODE_TABLE_PRIMARY 0x00
#define ZYDIS_OPCODE_TABLE_0F      0x01
#define ZYDIS_OPCODE_TABLE_0F38    0x02
#define ZYDIS_OPCODE_TABLE_0F3A    0x03
#define ZYDIS_OPCODE_TABLE_VEX     0x04
#define ZYDIS_OPCODE_TABLE_EVEX    0x14
#define ZYDIS_OPCODE_TABLE_MVEX    0x34
#define ZYDIS_OPCODE_TABLE_XOP     0x44
#define ZYDIS_OPCODE_TABLE_3DNOW   0x47



#pragma pack(pop)

#ifdef ZYAN_MSVC
#   pragma warning(pop)
#endif






typedef ZyanU8 ZydisInstructionEncodingFlags;


#define ZYDIS_INSTR_ENC_FLAG_HAS_MODRM      0x01


#define ZYDIS_INSTR_ENC_FLAG_HAS_DISP       0x02


#define ZYDIS_INSTR_ENC_FLAG_HAS_IMM0       0x04


#define ZYDIS_INSTR_ENC_FLAG_HAS_IMM1       0x08


#define ZYDIS_INSTR_ENC_FLAG_FORCE_REG_FORM 0x10


typedef struct ZydisInstructionEncodingInfo_
{
    
    ZydisInstructionEncodingFlags flags;
    
    struct
    {
        
        ZyanU8 size[3];
    } disp;
    
    struct
    {
        
        ZyanU8 size[3];
        
        ZyanBool is_signed;
        
        ZyanBool is_address;
        
        ZyanBool is_relative;
    } imm[2];
} ZydisInstructionEncodingInfo;












#define ZYDIS_DT_GET_TYPE(node) \
    ((ZydisDecoderTreeNodeType)((*(node)) & 0xFF))


#define ZYDIS_DT_GET_ARG0(node) \
    ((ZyanU8)(((*(node)) >> 8) & 0xFF))


#define ZYDIS_DT_GET_VALUE(node, index) \
    ((ZyanU16)(*((node) + 1 + (index))))


ZYDIS_NO_EXPORT const ZydisDecoderTreeNode* ZydisGetOpcodeTableRootNode(ZyanU8 opcode_table_id);


ZYDIS_NO_EXPORT void ZydisGetInstructionEncodingInfo(const ZydisDecoderTreeNode* node,
    const ZydisInstructionEncodingInfo** info);





#ifdef __cplusplus
}
#endif

#endif 

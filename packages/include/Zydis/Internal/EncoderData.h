

#ifndef ZYDIS_INTERNAL_ENCODERDATA_H
#define ZYDIS_INTERNAL_ENCODERDATA_H

#include <Zycore/Defines.h>
#include <Zydis/Mnemonic.h>
#include <Zydis/SharedTypes.h>
#include <Zydis/DecoderTypes.h>


typedef enum ZydisWidthFlag_
{
    ZYDIS_WIDTH_INVALID = 0x00,
    ZYDIS_WIDTH_16      = 0x01,
    ZYDIS_WIDTH_32      = 0x02,
    ZYDIS_WIDTH_64      = 0x04,

    
    ZYDIS_WIDTH_MAX_VALUE = (ZYDIS_WIDTH_64 | (ZYDIS_WIDTH_64 - 1)),
    
    ZYDIS_WIDTH_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_WIDTH_MAX_VALUE)
} ZydisWidthFlag;


typedef enum ZydisMandatoryPrefix_
{
    ZYDIS_MANDATORY_PREFIX_NONE,
    ZYDIS_MANDATORY_PREFIX_66,
    ZYDIS_MANDATORY_PREFIX_F2,
    ZYDIS_MANDATORY_PREFIX_F3,

    
    ZYDIS_MANDATORY_PREFIX_MAX_VALUE = ZYDIS_MANDATORY_PREFIX_F3,
    
    ZYDIS_MANDATORY_PREFIX_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_MANDATORY_PREFIX_MAX_VALUE)
} ZydisMandatoryPrefix;


typedef enum ZydisVectorLength_
{
    ZYDIS_VECTOR_LENGTH_INVALID,
    ZYDIS_VECTOR_LENGTH_128,
    ZYDIS_VECTOR_LENGTH_256,
    ZYDIS_VECTOR_LENGTH_512,

    
    ZYDIS_VECTOR_LENGTH_MAX_VALUE = ZYDIS_VECTOR_LENGTH_512,
    
    ZYDIS_VECTOR_LENGTH_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_VECTOR_LENGTH_MAX_VALUE)
} ZydisVectorLength;


typedef enum ZydisSizeHint_
{
    ZYDIS_SIZE_HINT_NONE,
    ZYDIS_SIZE_HINT_ASZ,
    ZYDIS_SIZE_HINT_OSZ,

    
    ZYDIS_SIZE_HINT_MAX_VALUE = ZYDIS_SIZE_HINT_OSZ,
    
    ZYDIS_SIZE_HINT_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_SIZE_HINT_MAX_VALUE)
} ZydisSizeHint;


typedef enum ZydisRex2Type_
{
    ZYDIS_REX2_TYPE_FORBIDDEN,
    ZYDIS_REX2_TYPE_ALLOWED,
    ZYDIS_REX2_TYPE_MANDATORY,

    
    ZYDIS_REX2_TYPE_MAX_VALUE = ZYDIS_REX2_TYPE_MANDATORY,
    
    ZYDIS_REX2_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_REX2_TYPE_MAX_VALUE)
} ZydisRex2Type;


typedef struct ZydisEncoderLookupEntry_
{
    
    ZyanU16 encoder_reference;
    
    ZyanU8 instruction_count;
} ZydisEncoderLookupEntry;

#pragma pack(push, 1)


typedef struct ZydisEncodableInstruction_
{
    
    ZyanU16 instruction_reference;
    
    ZyanU16 operand_mask;
    
    ZyanU8 opcode;
    
    ZyanU8 modrm;
    
    ZyanU8 encoding                 ZYAN_BITFIELD(ZYDIS_INSTRUCTION_ENCODING_REQUIRED_BITS);
    
    ZyanU8 opcode_map               ZYAN_BITFIELD(ZYDIS_OPCODE_MAP_REQUIRED_BITS);
    
    ZyanU8 modes                    ZYAN_BITFIELD(ZYDIS_WIDTH_REQUIRED_BITS);
    
    ZyanU8 address_sizes            ZYAN_BITFIELD(ZYDIS_WIDTH_REQUIRED_BITS);
    
    ZyanU8 operand_sizes            ZYAN_BITFIELD(ZYDIS_WIDTH_REQUIRED_BITS);
    
    ZyanU8 mandatory_prefix         ZYAN_BITFIELD(ZYDIS_MANDATORY_PREFIX_REQUIRED_BITS);
    
    ZyanU8 rex_w                    ZYAN_BITFIELD(1);
    
    ZyanU8 rex2                     ZYAN_BITFIELD(ZYDIS_REX2_TYPE_REQUIRED_BITS);
    
    ZyanU8 evex_nd                  ZYAN_BITFIELD(1);
    
    ZyanU8 evex_nf                  ZYAN_BITFIELD(1);
    
    ZyanU8 apx_osz                  ZYAN_BITFIELD(1);
    
    ZyanU8 vector_length            ZYAN_BITFIELD(ZYDIS_VECTOR_LENGTH_REQUIRED_BITS);
    
    ZyanU8 accepts_hint             ZYAN_BITFIELD(ZYDIS_SIZE_HINT_REQUIRED_BITS);
    
    ZyanU8 swappable                ZYAN_BITFIELD(1);
} ZydisEncodableInstruction;

#pragma pack(pop)


typedef struct ZydisEncoderRelInfo_
{
    
    ZyanU8 size[3][3];
    
    ZyanU8 accepts_scaling_hints;
    
    ZyanBool accepts_branch_hints;
    
    ZyanBool accepts_bound;
} ZydisEncoderRelInfo;


ZyanU8 ZydisGetEncodableInstructions(ZydisMnemonic mnemonic, 
    const ZydisEncodableInstruction **instruction);


const ZydisEncoderRelInfo *ZydisGetRelInfo(ZydisMnemonic mnemonic);


ZyanBool ZydisGetCcInfo(ZydisMnemonic mnemonic, ZydisSourceConditionCode *scc);

#endif 

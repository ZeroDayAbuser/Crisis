






























#ifndef ZYDIS_H
#define ZYDIS_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>

#if !defined(ZYDIS_DISABLE_DECODER)
#   include <Zydis/Decoder.h>
#   include <Zydis/DecoderTypes.h>
#endif

#if !defined(ZYDIS_DISABLE_ENCODER)
#   include <Zydis/Encoder.h>
#endif

#if !defined(ZYDIS_DISABLE_FORMATTER)
#   include <Zydis/Formatter.h>
#endif

#if !defined(ZYDIS_DISABLE_SEGMENT)
#   include <Zydis/Segment.h>
#endif

#if !defined(ZYDIS_DISABLE_DECODER) && !defined(ZYDIS_DISABLE_FORMATTER)
#   include <Zydis/Disassembler.h>
#endif

#include <Zydis/MetaInfo.h>
#include <Zydis/Mnemonic.h>
#include <Zydis/Register.h>
#include <Zydis/SharedTypes.h>
#include <Zydis/Status.h>
#include <Zydis/Utils.h>

#ifdef __cplusplus
extern "C" {
#endif




















#define ZYDIS_VERSION 0x0005000000000000ULL










#define ZYDIS_VERSION_MAJOR(version) (((version) & 0xFFFF000000000000) >> 48)






#define ZYDIS_VERSION_MINOR(version) (((version) & 0x0000FFFF00000000) >> 32)






#define ZYDIS_VERSION_PATCH(version) (((version) & 0x00000000FFFF0000) >> 16)






#define ZYDIS_VERSION_BUILD(version) ((version) & 0x000000000000FFFF)










typedef enum ZydisFeature_
{
    ZYDIS_FEATURE_DECODER,
    ZYDIS_FEATURE_ENCODER,
    ZYDIS_FEATURE_FORMATTER,
    ZYDIS_FEATURE_AVX512,
    ZYDIS_FEATURE_KNC,
    ZYDIS_FEATURE_SEGMENT,

    


    ZYDIS_FEATURE_MAX_VALUE = ZYDIS_FEATURE_SEGMENT,
    


    ZYDIS_FEATURE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_FEATURE_MAX_VALUE)
} ZydisFeature;













ZYDIS_EXPORT ZyanU64 ZydisGetVersion(void);









ZYDIS_EXPORT ZyanStatus ZydisIsFeatureEnabled(ZydisFeature feature);







#ifdef __cplusplus
}
#endif

#endif 

































#ifndef ZYDIS_SHORTSTRING_H
#define ZYDIS_SHORTSTRING_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>

#ifdef __cplusplus
extern "C" {
#endif





#if !defined(ZYAN_APPLE)
#   pragma pack(push, 1)
#endif









typedef struct ZydisShortString_
{
    


    const char* data;
    


    ZyanU8 size;
} ZydisShortString;

#if !defined(ZYAN_APPLE)
#   pragma pack(pop)
#endif










#define ZYDIS_MAKE_SHORTSTRING(string) \
    { string, sizeof(string) - 1 }



#ifdef __cplusplus
}
#endif

#endif 

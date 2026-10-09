






























#ifndef ZYDIS_METAINFO_H
#define ZYDIS_METAINFO_H

#include <Zydis/Defines.h>

#ifdef __cplusplus
extern "C" {
#endif





#include <Zydis/Generated/EnumInstructionCategory.h>
#include <Zydis/Generated/EnumISASet.h>
#include <Zydis/Generated/EnumISAExt.h>





 






ZYDIS_EXPORT const char* ZydisCategoryGetString(ZydisInstructionCategory category);








ZYDIS_EXPORT const char* ZydisISASetGetString(ZydisISASet isa_set);








ZYDIS_EXPORT const char* ZydisISAExtGetString(ZydisISAExt isa_ext);



#ifdef __cplusplus
}
#endif

#endif 

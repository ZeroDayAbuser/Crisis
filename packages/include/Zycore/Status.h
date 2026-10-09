




#ifndef ZYCORE_STATUS_H
#define ZYCORE_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Zycore/Types.h>




typedef ZyanU32 ZyanStatus;




#define ZYAN_MAKE_STATUS(error, module, code) \
    (ZyanStatus)((((error) & 0x01u) << 31u) | (((module) & 0x7FFu) << 20u) | ((code) & 0xFFFFFu))




#define ZYAN_SUCCESS(status) \
    (!((status) & 0x80000000u))


#define ZYAN_FAILED(status) \
    (!!((status) & 0x80000000u))


#define ZYAN_CHECK(status) \
    do \
    { \
        const ZyanStatus status_047620348 = (status); \
        if (!ZYAN_SUCCESS(status_047620348)) \
        { \
            return status_047620348; \
        } \
    } while (0)




#define ZYAN_STATUS_MODULE(status) \
    (((status) >> 20) & 0x7FFu)


#define ZYAN_STATUS_CODE(status) \
    ((status) & 0xFFFFFu)




#define ZYAN_MODULE_ZYCORE      0x001u


#define ZYAN_MODULE_ARGPARSE    0x003u


#define ZYAN_MODULE_USER        0x3FFu




#define ZYAN_STATUS_SUCCESS \
    ZYAN_MAKE_STATUS(0u, ZYAN_MODULE_ZYCORE, 0x00u)


#define ZYAN_STATUS_FAILED \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x01u)


#define ZYAN_STATUS_TRUE \
    ZYAN_MAKE_STATUS(0u, ZYAN_MODULE_ZYCORE, 0x02u)


#define ZYAN_STATUS_FALSE \
    ZYAN_MAKE_STATUS(0u, ZYAN_MODULE_ZYCORE, 0x03u)


#define ZYAN_STATUS_INVALID_ARGUMENT \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x04u)


#define ZYAN_STATUS_INVALID_OPERATION \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x05u)


#define ZYAN_STATUS_ACCESS_DENIED \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x06u)


#define ZYAN_STATUS_NOT_FOUND \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x07u)


#define ZYAN_STATUS_OUT_OF_RANGE \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x08u)


#define ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x09u)


#define ZYAN_STATUS_NOT_ENOUGH_MEMORY \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x0Au)


#define ZYAN_STATUS_BAD_SYSTEMCALL \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x0Bu)


#define ZYAN_STATUS_OUT_OF_RESOURCES \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x0Cu)


#define ZYAN_STATUS_MISSING_DEPENDENCY \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ZYCORE, 0x0Du)




#define ZYAN_STATUS_ARG_NOT_UNDERSTOOD \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ARGPARSE, 0x00u)


#define ZYAN_STATUS_TOO_FEW_ARGS \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ARGPARSE, 0x01u)


#define ZYAN_STATUS_TOO_MANY_ARGS \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ARGPARSE, 0x02u)


#define ZYAN_STATUS_ARG_MISSES_VALUE \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ARGPARSE, 0x03u)


#define ZYAN_STATUS_REQUIRED_ARG_MISSING \
    ZYAN_MAKE_STATUS(1u, ZYAN_MODULE_ARGPARSE, 0x04u)




#ifdef __cplusplus
}
#endif

#endif

#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

/* Includes definition of mch_printf macro to do printf */
#include "mch.h"

/* #define BYTE_ORDER  BIG_ENDIAN */

typedef uint8_t     u8_t;
typedef int8_t      s8_t;
typedef uint16_t    u16_t;
typedef int16_t     s16_t;
typedef uint32_t    u32_t;
typedef int32_t     s32_t;

typedef uintptr_t   mem_ptr_t;

#define LWIP_ERR_T  int

/* Define (sn)printf formatters for these lwIP types */
#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"

/* Compiler hints for packing structures */
#define PACK_STRUCT_FIELD(x)    x
#define PACK_STRUCT_STRUCT  __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Plaform specific diagnostic output */
#define LWIP_PLATFORM_DIAG(x)   do {                \
        printf x;                   \
    } while (0)

#define LWIP_PLATFORM_ASSERT(x) do {                \
        printf("Assert \"%s\" failed at line %d in %s\n",   \
                x, __LINE__, __FILE__);             \
        mch_abort();                        \
    } while (0)

#if !USE_FREERTOS
static inline u32_t sys_now(void) { return 0; /* return read_rtc() * 1000U; */ }
#endif

#define LWIP_RAND() ((u32_t)rand())

#define LWIP_TIMEVAL_PRIVATE 0

#endif /* __ARCH_CC_H__ */

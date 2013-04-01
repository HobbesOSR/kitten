#include <lwk/types.h>
#include <lwk/print.h>
#include <arch/string.h>
#include <lwk/time.h>

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

typedef int8_t s8_t;
typedef int16_t s16_t;
typedef int32_t s32_t;
typedef int64_t s64_t;

#define S16_F "hd"
#define U16_F "hu"
#define X16_F "hx"
#define S32_F "d"
#define U32_F "u"
#define X32_F "x"
#define S64_F "ld"
#define U64_F "lu"
#define X64_F "lx"

typedef uintptr_t mem_ptr_t;

#define BYTE_ORDER LITTLE_ENDIAN

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_DIAG(x) printk x
#define LWIP_PLATFORM_ASSERT(x) printk( "!!!!! lwip: %s\n", x )

#define SYS_ARCH_DECL_PROTECT(x)
#define SYS_ARCH_PROTECT(x)
#define SYS_ARCH_UNPROTECT(x)

#define PERF_START do { } while(0)
#define PERF_STOP(x) do {} while(0)


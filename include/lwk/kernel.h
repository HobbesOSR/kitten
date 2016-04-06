#ifndef _LWK_KERNEL_H
#define _LWK_KERNEL_H

/*
 * 'kernel.h' contains some often used function prototypes etc
 */

#include <stdarg.h>
#include <lwk/macros.h>
#include <lwk/linkage.h>
#include <lwk/stddef.h>
#include <lwk/types.h>
#include <lwk/compiler.h>
#include <lwk/bitops.h>
#include <lwk/kmem.h>
#include <lwk/errno.h>
#include <lwk/utsname.h>
#include <lwk/print.h>
#include <lwk/log2.h>
#include <lwk/percpu.h>
#include <arch/byteorder.h>
#include <arch/bug.h>

#include <lwk/print.h>
#define _KDBG(fmt, args...) \
print("(%d)%s:%s(%i) "fmt,current->id,__FILE__,__FUNCTION__, __LINE__, ## args)

extern int __flag;
#define __KDBG(fmt, args...) if ( current->id != 0  )\
print("(%d)%s:%s(%i) "fmt,current->id,__FILE__,__FUNCTION__, __LINE__, ## args)


extern const char lwk_banner[];

#define INT_MAX         ((int)(~0U>>1))
#define INT_MIN         (-INT_MAX - 1)
#define UINT_MAX        (~0U)
#define LONG_MAX        ((long)(~0UL>>1))
#define LONG_MIN        (-LONG_MAX - 1)
#define ULONG_MAX       (~0UL)
#define INT64_MAX       ((long long)(~0ULL>>1))
#define UINT64_MAX      (~0ULL)

#define STACK_MAGIC     0xdeadbeef

#define CACHE_LINE_SIZE 128

extern void panic(const char * fmt, ...) __noreturn;

extern unsigned long simple_strtoul(const char *,char **,unsigned int);
extern long simple_strtol(const char *,char **,unsigned int);
extern unsigned long long simple_strtoull(const char *,char **,unsigned int);
extern long long simple_strtoll(const char *,char **,unsigned int);
extern int get_option(char **str, int *pint);
extern char *get_options(const char *str, int nints, int *ints);
extern unsigned long long memparse(char *ptr, char **retptr);

extern int sscanf(const char *, const char *, ...)
	__attribute__ ((format (scanf, 2, 3)));
extern int vsscanf(const char *, const char *, va_list)
	__attribute__ ((format (scanf, 2, 0)));

DECLARE_PER_CPU(bool, umem_only);

#endif

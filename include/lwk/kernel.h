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
#include <lwk/kmem.h>
#include <lwk/errno.h>
#include <lwk/utsname.h>
#include <lwk/print.h>
#include <arch/byteorder.h>
#include <arch/bug.h>

extern const char lwk_banner[];
extern struct utsname linux_utsname;

#define INT_MAX         ((int)(~0U>>1))
#define INT_MIN         (-INT_MAX - 1)
#define UINT_MAX        (~0U)
#define LONG_MAX        ((long)(~0UL>>1))
#define LONG_MIN        (-LONG_MAX - 1)
#define ULONG_MAX       (~0UL)

#define STACK_MAGIC     0xdeadbeef

void panic(const char * fmt, ...);

extern unsigned long simple_strtoul(const char *,char **,unsigned int);
extern long simple_strtol(const char *,char **,unsigned int);
extern unsigned long long simple_strtoull(const char *,char **,unsigned int);
extern long long simple_strtoll(const char *,char **,unsigned int);
extern int get_option(char **str, int *pint);
extern char *get_options(const char *str, int nints, int *ints);
extern unsigned long long memparse(char *ptr, char **retptr);

#endif

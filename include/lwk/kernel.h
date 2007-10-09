#ifndef _LWK_KERNEL_H
#define _LWK_KERNEL_H

/*
 * 'kernel.h' contains some often used function prototypes etc
 */

#include <stdarg.h>
#include <lwk/linkage.h>
#include <lwk/stddef.h>
#include <lwk/types.h>
#include <lwk/compiler.h>
#include <arch/byteorder.h>
#include <arch/bug.h>

extern const char lwk_banner[];

#define INT_MAX         ((int)(~0U>>1))
#define INT_MIN         (-INT_MAX - 1)
#define UINT_MAX        (~0U)
#define LONG_MAX        ((long)(~0UL>>1))
#define LONG_MIN        (-LONG_MAX - 1)
#define ULONG_MAX       (~0UL)

#define STACK_MAGIC     0xdeadbeef

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ALIGN(x,a) (((x)+(a)-1)&~((a)-1))
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define KERN_EMERG	"<0>"	/* system is unusable                   */
#define KERN_ALERT	"<1>"	/* action must be taken immediately     */
#define KERN_CRIT	"<2>"	/* critical conditions                  */
#define KERN_ERR	"<3>"	/* error conditions                     */
#define KERN_WARNING	"<4>"	/* warning conditions                   */
#define KERN_NOTICE	"<5>"	/* normal but significant condition     */
#define KERN_INFO	"<6>"	/* informational                        */
#define KERN_DEBUG	"<7>"	/* debug-level messages                 */

void panic(const char * fmt, ...);

extern unsigned long simple_strtoul(const char *,char **,unsigned int);
extern long simple_strtol(const char *,char **,unsigned int);
extern unsigned long long simple_strtoull(const char *,char **,unsigned int);
extern long long simple_strtoll(const char *,char **,unsigned int);
extern int sprintf(char * buf, const char * fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
extern int vsprintf(char *buf, const char *, va_list)
	__attribute__ ((format (printf, 2, 0)));
extern int snprintf(char * buf, size_t size, const char * fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
	__attribute__ ((format (printf, 3, 0)));
extern int scnprintf(char * buf, size_t size, const char * fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
extern int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
	__attribute__ ((format (printf, 3, 0)));
extern int vprintk(const char *fmt, va_list args)
        __attribute__ ((format (printf, 1, 0)));
extern int printk(const char * fmt, ...)
        __attribute__ ((format (printf, 1, 2)));

extern int get_option(char **str, int *pint);
extern char *get_options(const char *str, int nints, int *ints);
extern unsigned long long memparse(char *ptr, char **retptr);

/*
 * min()/max() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; })

#define max(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x > _y ? _x : _y; })

/*
 * ..and if you can't take the strict
 * types, you can specify one yourself.
 *
 * Or not use min/max at all, of course.
 */
#define min_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#define max_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })

/*
 * Round x up to the nearest y aligned boundary.  y must be a power of two.
 */
#define round_up(x,y) (((x) + (y) - 1) & ~((y)-1))

/*
 * Round x down to the nearest y aligned boundary.  y must be a power of two.
 */
#define round_down(x,y) ((x) & ~((y)-1))

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})

/*
 * Check at compile time that something is of a particular type.
 * Always evaluates to 1 so you may use it easily in comparisons.
 */
#define typecheck(type,x) \
({	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})

/*
 * Check at compile time that 'function' is a certain type, or is a pointer
 * to that type (needs to use typedef for the function type.)
 */
#define typecheck_fn(type,function) \
({	typeof(type) __tmp = function; \
	(void)__tmp; \
})

/*
 * Check at compile time that 'type' is a multiple of align.
 */
#define aligncheck(type,align) \
	extern int __align_check[ (sizeof(type) % (align) == 0 ? 0 : 1/0) ]

/*
 * Check at compile time that the type 'type' has the expected 'size'.
 * NOTE: 'type' must be a single word, so this doesn't work for structures.
 *       For structures, use sizecheck_struct().
 */
#define sizecheck(type,size) \
	extern int __size_check_##type[ (sizeof(type) == (size) ? 0 : 1/0) ]

/*
 * Check at compile time that the structure 'name' has the expected size 'size'.
 */
#define sizecheck_struct(name,size) \
	extern int __size_check_struct_##name[ (sizeof(struct name) == (size) ? 0 : 1/0) ]

/*
 * Force a compilation error if condition is true
 */
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

#endif

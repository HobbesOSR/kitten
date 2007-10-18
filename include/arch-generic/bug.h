#ifndef _ARCH_GENERIC_BUG_H
#define _ARCH_GENERIC_BUG_H

#include <lwk/compiler.h>

#ifndef HAVE_ARCH_BUG
#define BUG() do { \
	printk("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __FUNCTION__); \
	panic("BUG!"); \
} while (0)
#endif

#ifndef HAVE_ARCH_BUG_ON
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)
#endif

#ifndef HAVE_ARCH_ASSERT
#define ASSERT(condition) do { if (unlikely((condition)!=1)) BUG(); } while(0)
#endif

#ifndef HAVE_ARCH_WARN_ON
#define WARN_ON(condition) do { \
	if (unlikely((condition)!=0)) { \
		printk("BUG: warning at %s:%d/%s()\n", __FILE__, __LINE__, __FUNCTION__); \
		/* TODO FIX ME */ \
		/* dump_stack(); */ \
	} \
} while (0)
#endif

#endif

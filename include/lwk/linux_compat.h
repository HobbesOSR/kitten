/** \file
 * 
 * Linux wrappers.
 *
 * Mostly Linux stuff that is not implemented or supported in LWK, but
 * that we emulate to make importing sources from Linux easier.
 */
#ifndef _LWK_LINUX_COMPAT_H
#define _LWK_LINUX_COMPAT_H

#include <arch/system.h>
#include <arch/bug.h>


#define EXPORT_SYMBOL(sym)
#define EXPORT_SYMBOL_GPL(sym)
#define EXPORT_SYMBOL_GPL_FUTURE(sym)
#define EXPORT_PER_CPU_SYMBOL(var) EXPORT_SYMBOL(per_cpu__##var)
#define EXPORT_PER_CPU_SYMBOL_GPL(var) EXPORT_SYMBOL_GPL(per_cpu__##var)

#define pr_fmt(fmt) "(DEBUG) %s:%d: " fmt, __func__, __LINE__
#if defined(DEBUG)
#define pr_debug(fmt, ...) \
	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) \
	({ if (0) printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__); 0; })
#endif

#define WARN(level,fmt, ...) \
	printk(KERN_WARNING "(WARNING) %s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__ )

/* The lwk always zeros memory */
#define kzalloc( size, unused) kmem_alloc(size)
#define kmalloc( size, unused) kmem_alloc(size)
#define kfree( ptr ) kmem_free(ptr)

/* We only support stack dumps if the kernel is compiled with it */
extern void kstack_trace( void * context );
#define dump_stack() kstack_trace(0)

#endif

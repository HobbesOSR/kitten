/** \file
 * Linux compatability macros.
 *
 * Kitten is not 100% Linux compatible, but to ease transition
 * of Linux kernel modules some macros are provided to avoid
 * rewriting too much code.
 */
#ifndef __lwk_linux_h__
#define __lwk_linux_h__

#include <lwk/print.h>
#include <lwk/kmem.h>
#include <arch/bug.h>

// LWK always zeros memory
#define kmalloc( size, type ) kmem_alloc( size )
#define kzalloc( size, type ) kmem_alloc( size ) 
#define kfree kmem_free

#define GFP_KERNEL 0
#define EXPORT_SYMBOL(x)


#if 1 //def DEBUG
#define pr_debug(fmt,arg...) \
        printk(KERN_DEBUG fmt,##arg)
#else
#define pr_debug(fmt,arg...) \
        do { } while (0)
#endif


#endif

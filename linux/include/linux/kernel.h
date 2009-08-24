#ifndef __LINUX_KERNEL_H
#define __LINUX_KERNEL_H

#include <lwk/kernel.h>
#include <lwk/show.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/byteorder.h>
#include <linux/rwsem.h>
#include <linux/topology.h>

extern char *kasprintf(gfp_t gfp, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
extern char *kvasprintf(gfp_t gfp, const char *fmt, va_list args);

/**
 * upper_32_bits - return bits 32-63 of a number
 * @n: the number we're accessing
 *
 * A basic shift-right of a 64- or 32-bit quantity.  Use this to suppress
 * the "right shift count >= width of type" warning when that quantity is
 * 32-bits.
 */
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))

/**
 * lower_32_bits - return bits 0-31 of a number
 * @n: the number we're accessing
 */
#define lower_32_bits(n) ((u32)(n))

#ifdef DEBUG
/* If you are writing a driver, please use dev_dbg instead */
#define pr_debug(fmt, arg...) \
	printk(KERN_DEBUG fmt, ##arg)
#else
#define pr_debug(fmt, arg...) \
	({ if (0) printk(KERN_DEBUG fmt, ##arg); 0; })
#endif

#define dump_stack() show_kstack()

#endif

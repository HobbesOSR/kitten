#ifndef __LINUX_KERNEL_H
#define __LINUX_KERNEL_H

#include <lwk/kernel.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/byteorder.h>

extern char *kasprintf(gfp_t gfp, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
extern char *kvasprintf(gfp_t gfp, const char *fmt, va_list args);

#endif

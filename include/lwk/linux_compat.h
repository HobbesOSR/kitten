#ifndef _LWK_LINUX_COMPAT_H
#define _LWK_LINUX_COMPAT_H

#include <linux/module.h>

#ifdef CONFIG_LINUX
extern void linux_init(void);
#else
static inline void linux_init(void) {}
#endif

#endif

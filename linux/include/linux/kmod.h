
#ifndef _LINUX_KMOD_H
#define _LINUX_KMOD_H

#include <linux/lwk_stubs.h>

static inline int request_module(const char * name, ...) {
	LINUX_DBG(FALSE,"name \"%s\"\n",name);
    	return 0;
}

#endif

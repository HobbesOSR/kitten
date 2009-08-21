#ifndef _LINUX_RANDOM_H
#define _LINUX_RANDOM_H

#include <lwk/random.h>

static inline void
get_random_bytes(void *buf, int num_bytes)
{
	rand_get_bytes(buf, num_bytes);
}

#endif

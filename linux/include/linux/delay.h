#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

#include <lwk/delay.h>

static inline void
msleep(unsigned int msecs)
{
	__udelay(msecs * 1000);
}

#endif

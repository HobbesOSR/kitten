#ifndef __LINUX_RWSEM_H
#define __LINUX_RWSEM_H

#include <lwk/semaphore.h>

#define rw_semaphore		semaphore

#define DECLARE_RWSEM(name)	DECLARE_MUTEX((name))

#define down_read		down
#define down_write		down

#define up_read			up
#define up_write		up

#endif

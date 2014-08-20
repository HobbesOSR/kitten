/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUX_WAIT_H_
#define	_LINUX_WAIT_H_

#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/list.h>

#include <lwk/waitq.h>



#define wait_queue_head_t      waitq_t
#define	init_waitqueue_head(x) waitq_init(x)

#define	wake_up(q)				waitq_wake_nr(q, 1)
#define	wake_up_nr(q, nr)			waitq_wake_nr(q, nr)
#define	wake_up_all(q)				waitq_wakeup(q)
#define	wake_up_interruptible(q)		waitq_wake_nr(q, 1)
#define	wake_up_interruptible_nr(q, nr)		waitq_wake_nr(q, nr)
#define	wake_up_interruptible_all(q, nr)        waitq_wakeup(q)


/* JRL: These do not appear to be needed

#define __wait_event_interruptible_timeout(wq, condition, ret)          \
    do {								\
	DEFINE_WAIT(__wait);                                            \
									\
	for (;;) {                                                      \
	    prepare_to_wait(&wq, &__wait, TASK_INTERRUPTIBLE);		\
	    if (condition)						\
		break;							\
	    if (!signal_pending(current)) {				\
		ret = schedule_timeout(ret);				\
		if (!ret)						\
		    break;						\
		continue;						\
	    }								\
	    ret = -ERESTARTSYS;						\
	    break;							\
	}                                                               \
	finish_wait(&wq, &__wait);                                      \
    } while (0)

#define wait_event_interruptible_timeout(wq, condition, timeout)        \
    ({									\
	long __ret = timeout;                                           \
	if (!(condition))                                               \
	    __wait_event_interruptible_timeout(wq, condition, __ret);	\
	__ret;                                                          \
    })


*/

#define	DEFINE_WAIT(x) DECLARE_WAITQ_ENTRY((x), current)

#endif	/* _LINUX_WAIT_H_ */

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
#ifndef _LINUX_TIMER_H_
#define _LINUX_TIMER_H_

#include <linux/types.h>

#include <lwk/timer.h>
#include <lwk/smp.h>


#define timer_list timer


#define DEFINE_TIMER(_name, _function, _expires, _data)			\
	struct timer_list _name =					\
		TIMER_INITIALIZER(_name, _function, _expires, _data)

/** ** **/
static inline void 
init_timer(struct timer_list *timer) 
{
	list_head_init(&timer->link);
	timer->cpu      = this_cpu;
}

static inline void
init_timer_deferrable( struct timer_list *timer )
{
	init_timer(timer);
}



static inline void setup_timer(struct timer_list * timer,
			       void (*function)(unsigned long),
			       unsigned long data)
{
	timer->function = function;
	timer->data = data;
	init_timer(timer);
}


static inline int
del_timer( struct timer_list * timer )
{
	int ret = 0;
	if ( timer_pending(timer) ) {
		if ( ( ret = timer_del(timer) ) ) {
			list_head_init(&timer->link);
		}
	}
	return ret;
}

static inline int
del_timer_sync( struct timer_list * timer )
{
	return del_timer(timer);
}



int
__mod_timer( struct timer_list *             timer,
	     unsigned long                   expires )
{
	int not_expired = 0;
	
	BUG_ON(!timer->function);
	
	not_expired = del_timer(timer);
	
	timer->expires = expires;
	
	timer->cpu      = this_cpu;
	timer->expires  = expires;
	
	timer_add(timer);
	
	return not_expired;
}

int
mod_timer( struct timer_list *             timer,
	   unsigned long                   expires )
{
	if (timer_pending(timer) && timer->expires == expires)
		return 1;
	
	return __mod_timer(timer, expires);
}


/**
 * add_timer - start a timer
 * @timer: the timer to be added
 *
 * The kernel will do a ->function(->data) callback from the
 * timer interrupt at the ->expires point in the future. The
 * current time is 'jiffies'.
 *
 * The timer's ->expires, ->function (and if the handler uses it, ->data)
 * fields must be set prior calling this function.
 *
 * Timers with an ->expires field in the past will be executed in the next
 * timer tick.
 */
static inline void add_timer(struct timer_list *timer)
{
	BUG_ON(timer_pending(timer));
	__mod_timer(timer, timer->expires);
}



/**
 * This is supposed to round the input jiffies value to a multiple
 * of one second.  Linux does a whole bunch of stuff to add some
 * skew for each CPU and what not.  The LWK doesn't bother and
 * assumes the caller passed in something fairly close to being
 * a multiple of a second.
 */
static inline unsigned long
round_jiffies(unsigned long j)
{
	return j;
}

#endif /* _LINUX_TIMER_H_ */

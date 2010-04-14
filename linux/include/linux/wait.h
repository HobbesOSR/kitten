/**
 * \file
 * This maps Linux wait queues to LWK wait queues.
 */

#ifndef __LINUX_WAIT_H
#define __LINUX_WAIT_H

#include <lwk/waitq.h>

#define wait_queue_head_t		waitq_t
#define wait_queue_t			waitq_entry_t
#define wait_queue_func_t		waitq_func_t

#define DECLARE_WAIT_QUEUE_HEAD(name)	DECLARE_WAITQ((name))
#define DECLARE_WAITQUEUE(name, tsk)	DECLARE_WAITQ_ENTRY((name), (tsk))

#define DEFINE_WAIT(name)			DECLARE_WAITQ_ENTRY((name), current)

#define init_waitqueue_head(q)      waitq_init(q)
#define wake_up(x)                  waitq_wake_nr(x, 1)
#define wake_up_interruptible(x)	waitq_wake_nr(x, 1)
#define wake_up_all(x)              waitq_wakeup(x)

#define __add_wait_queue(q, e)	    waitq_add_entry_locked(q, e)
#define __remove_wait_queue(q, e)   waitq_remove_entry_locked(q, e)

#define add_wait_queue(q, e)	    waitq_add_entry(q, e)
#define remove_wait_queue(q, e)	    waitq_remove_entry(q, e)

/* moved to lwk/waitq.h
int default_wake_function(wait_queue_t *wait, unsigned mode, int flags,
						  void *key);
*/
static inline void init_waitqueue_func_entry(wait_queue_t *q,
											 wait_queue_func_t func)
{
	q->private = NULL;
	q->func = func;
	list_head_init(&q->link);
}
static inline void init_waitqueue_entry(wait_queue_t *q, struct task_struct *p)
{
	q->private = p;
	q->func = default_wake_function;
	list_head_init(&q->link);
}

#define __wait_event_timeout(wq, condition, ret)                        \
do {                                                                    \
	DEFINE_WAIT(__wait);                                            \
									\
	for (;;) {                                                      \
		prepare_to_wait(&wq, &__wait, TASK_UNINTERRUPTIBLE);    \
		if (condition)                                          \
			break;                                          \
		ret = schedule_timeout(ret);                            \
		if (!ret)                                               \
			break;                                          \
	}                                                               \
	finish_wait(&wq, &__wait);                                      \
} while (0)


#define wait_event_timeout(wq, condition, timeout)                      \
({                                                                      \
	long __ret = timeout;                                           \
	if (!(condition))                                               \
		__wait_event_timeout(wq, condition, __ret);             \
	__ret;                                                          \
})

#define __wait_event_interruptible_timeout(wq, condition, ret)          \
do {                                                                    \
	DEFINE_WAIT(__wait);                                            \
									\
	for (;;) {                                                      \
		prepare_to_wait(&wq, &__wait, TASK_INTERRUPTIBLE);      \
		if (condition)                                          \
			break;                                          \
		if (!signal_pending(current)) {                         \
			ret = schedule_timeout(ret);                    \
			if (!ret)                                       \
				break;                                  \
			continue;                                       \
		}                                                       \
		ret = -ERESTARTSYS;                                     \
		break;                                                  \
	}                                                               \
	finish_wait(&wq, &__wait);                                      \
} while (0)

#define wait_event_interruptible_timeout(wq, condition, timeout)        \
({                                                                      \
	long __ret = timeout;                                           \
	if (!(condition))                                               \
		__wait_event_interruptible_timeout(wq, condition, __ret); \
	__ret;                                                          \
})

void prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait, int state);
void finish_wait(wait_queue_head_t *q, wait_queue_t *wait);

#endif

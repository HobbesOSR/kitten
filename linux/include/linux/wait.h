/**
 * \file
 * This maps Linux wait queues to LWK wait queues.
 */

#ifndef __LINUX_WAIT_H
#define __LINUX_WAIT_H

#include <lwk/waitq.h>

#define wait_queue_head_t		waitq_t
#define wait_queue_t			waitq_entry_t

#define DECLARE_WAIT_QUEUE_HEAD(name)	DECLARE_WAITQ((name))
#define DECLARE_WAITQUEUE(name, tsk)	DECLARE_WAITQ_ENTRY((name), (tsk))

#define DEFINE_WAIT(name)		DECLARE_WAITQ_ENTRY((name), current)

#define init_waitqueue_head		waitq_init
#define wake_up				waitq_wakeup
#define wake_up_interruptible		waitq_wakeup
#define wake_up_all			waitq_wakeup

#define __add_wait_queue		waitq_add_entry_locked
#define __remove_wait_queue		waitq_remove_entry_locked

#define add_wait_queue			waitq_add_entry
#define remove_wait_queue		waitq_remove_entry

#endif

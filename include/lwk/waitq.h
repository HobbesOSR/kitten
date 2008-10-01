#ifndef _LWK_WAITQ_H
#define _LWK_WAITQ_H

#include <lwk/spinlock.h>
#include <lwk/list.h>
#include <lwk/task.h>

typedef struct waitq {
	spinlock_t           lock;
	struct list_head     waitq;
} waitq_t;

typedef struct waitq_entry {
	struct task_struct * task;
	struct list_head     link;
} waitq_entry_t;

#define DECLARE_WAITQ(name)                             \
	waitq_t name = {                                \
	    .lock  = SPIN_LOCK_UNLOCKED,                \
	    .waitq = { &(name).waitq, &(name).waitq }   \
	}

#define DECLARE_WAITQ_ENTRY(name, tsk)                  \
	waitq_entry_t name = {                          \
	    .task  = tsk,                               \
	    .link  = { &(name).link, &(name).link }     \
	}

extern void waitq_init(waitq_t *waitq);
extern void waitq_init_entry(waitq_entry_t *entry, struct task_struct *task);
extern bool waitq_active(waitq_t *waitq);
extern void waitq_add_entry(waitq_t *waitq, waitq_entry_t *entry);
extern void waitq_prepare_to_wait(waitq_t *waitq, waitq_entry_t *entry,
                                  taskstate_t state);
extern void waitq_finish_wait(waitq_t *waitq, waitq_entry_t *entry);
extern void waitq_wakeup(waitq_t *waitq);

/**
 * This puts the task to sleep until condition becomes true.
 * This must be a macro because condition is tested repeatedly, not just
 * when wait_event() is first called.
 */
#define wait_event(waitq, condition)                                           \
do {                                                                           \
	DECLARE_WAITQ_ENTRY(__entry, current);                                 \
	for (;;) {                                                             \
		prepare_to_wait(&waitq, &__entry, TASKSTATE_UNINTERRUPTIBLE);  \
		if (condition)                                                 \
			break;                                                 \
		schedule();                                                    \
	}                                                                      \
	finish_wait(&waitq, &__entry);                                         \
} while (0)

#endif

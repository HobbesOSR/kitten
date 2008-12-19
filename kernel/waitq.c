#include <lwk/kernel.h>
#include <lwk/waitq.h>
#include <lwk/sched.h>

void
waitq_init(waitq_t *waitq)
{
	spin_lock_init(&waitq->lock);
	list_head_init(&waitq->waitq);
}

void
waitq_init_entry(waitq_entry_t *entry, struct task_struct *task)
{
	entry->task = task;
	list_head_init(&entry->link);
}

bool
waitq_active(waitq_t *waitq)
{
	bool active;
	unsigned long irqstate;

	spin_lock_irqsave(&waitq->lock, irqstate);
	active = !list_empty(&waitq->waitq);
	spin_unlock_irqrestore(&waitq->lock, irqstate);

	return active;
}

void
waitq_add_entry(waitq_t *waitq, waitq_entry_t *entry)
{
	unsigned long irqstate;

	spin_lock_irqsave(&waitq->lock, irqstate);
	BUG_ON(!list_empty(&entry->link));
	list_add(&entry->link, &waitq->waitq);
	spin_unlock_irqrestore(&waitq->lock, irqstate);
}


void
waitq_add_entry_locked(waitq_t *waitq, waitq_entry_t *entry)
{
	list_add(&entry->link, &waitq->waitq);
}


void
waitq_remove_entry(waitq_t *waitq, waitq_entry_t *entry)
{
	unsigned long irqstate;

	spin_lock_irqsave(&waitq->lock, irqstate);
	BUG_ON(list_empty(&entry->link));
	list_del_init(&entry->link);
	spin_unlock_irqrestore(&waitq->lock, irqstate);
}


void
waitq_remove_entry_locked(waitq_t *waitq, waitq_entry_t *entry)
{
	BUG_ON(list_empty(&entry->link));
	list_del_init(&entry->link);
}



void
waitq_prepare_to_wait(waitq_t *waitq, waitq_entry_t *entry, taskstate_t state)
{
	unsigned long irqstate;

	spin_lock_irqsave(&waitq->lock, irqstate);
	if (list_empty(&entry->link))
		list_add(&entry->link, &waitq->waitq);
	set_mb(entry->task->state, state);
	spin_unlock_irqrestore(&waitq->lock, irqstate);
}

void
waitq_finish_wait(waitq_t *waitq, waitq_entry_t *entry)
{
	set_mb(entry->task->state, TASKSTATE_READY);
	waitq_remove_entry(waitq, entry);
}

void
waitq_wakeup(waitq_t *waitq)
{
	unsigned long irqstate;
	struct list_head *tmp;
	waitq_entry_t *entry;

	spin_lock_irqsave(&waitq->lock, irqstate);
	list_for_each(tmp, &waitq->waitq) {
		entry  = list_entry(tmp, waitq_entry_t, link);
		sched_wakeup_task(
			entry->task,
			(TASKSTATE_UNINTERRUPTIBLE | TASKSTATE_INTERRUPTIBLE)
		);
	}
	spin_unlock_irqrestore(&waitq->lock, irqstate);
}


void
waitq_wakeone( waitq_t * waitq )
{
	unsigned long irqstate;

	spin_lock_irqsave(&waitq->lock, irqstate);
	waitq_wakeone_locked( waitq );
	spin_unlock_irqrestore(&waitq->lock, irqstate);
}


void
waitq_wakeone_locked( waitq_t * waitq )
{
	struct list_head * list = &waitq->waitq;
	if( list_empty( list ) )
		return;

	waitq_entry_t *entry = list_entry( &list->next, waitq_entry_t, link);
	sched_wakeup_task(
		entry->task,
		(TASKSTATE_UNINTERRUPTIBLE | TASKSTATE_INTERRUPTIBLE)
	);
}


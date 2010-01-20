#include <lwk/kernel.h>
#include <lwk/waitq.h>
#include <lwk/sched.h>

int
default_wake_function(waitq_entry_t *entry, unsigned mode,
					  int flags, void *key)
{
	return sched_wakeup_task(entry->private, TASK_NORMAL);
}

void
waitq_init(waitq_t *waitq)
{
	spin_lock_init(&waitq->lock);
	list_head_init(&waitq->waitq);
}

void
waitq_init_entry(waitq_entry_t *entry, struct task_struct *task)
{
	entry->private = task;
	entry->func = default_wake_function;
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
	waitq_add_entry_locked( waitq, entry );
	spin_unlock_irqrestore(&waitq->lock, irqstate);
}


void
waitq_add_entry_locked(waitq_t *waitq, waitq_entry_t *entry)
{
	BUG_ON(!list_empty(&entry->link));
	list_add_tail(&entry->link, &waitq->waitq);
}


void
waitq_remove_entry(waitq_t *waitq, waitq_entry_t *entry)
{
	unsigned long irqstate;

	spin_lock_irqsave(&waitq->lock, irqstate);
	waitq_remove_entry_locked( waitq, entry );
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
	set_current_state(state);
	spin_unlock_irqrestore(&waitq->lock, irqstate);
}

void
waitq_finish_wait(waitq_t *waitq, waitq_entry_t *entry)
{
	__set_current_state(TASK_RUNNING);
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
		entry->func(entry, 0, 0, NULL);
	}
	spin_unlock_irqrestore(&waitq->lock, irqstate);
}


int
waitq_wake_nr( waitq_t * waitq, int nr )
{
	unsigned long irqstate;

	spin_lock_irqsave(&waitq->lock, irqstate);
	int count = waitq_wake_nr_locked( waitq, nr );
	spin_unlock_irqrestore(&waitq->lock, irqstate);

	if( count > 0 )
		schedule();

	return count;
}


int
waitq_wake_nr_locked( waitq_t * waitq, int nr )
{
	int count = 0;
	waitq_entry_t *entry;

	list_for_each_entry(entry, &waitq->waitq, link) {
		if( ++count > nr )
			break;
		entry->func(entry, 0, 0, NULL);
	}

	return count - 1;
}

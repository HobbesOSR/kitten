#include <lwk/linux_compat.h>

#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/wait.h>


void
prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait, int state)
{
	unsigned long flags;

	_KDBG("\n");
	wait->flags &= ~WQ_FLAG_EXCLUSIVE;
	spin_lock_irqsave(&q->lock, flags);
	if (list_empty(&wait->task_list))
		__add_wait_queue(q, wait);
	set_current_state(state);
	spin_unlock_irqrestore(&q->lock, flags);
}

void finish_wait(wait_queue_head_t *q, wait_queue_t *wait)
{
	unsigned long flags;

	_KDBG("\n");
	__set_current_state(TASK_RUNNING);

	if (!list_empty_careful(&wait->task_list)) {
		spin_lock_irqsave(&q->lock, flags);
		list_del_init(&wait->task_list);
		spin_unlock_irqrestore(&q->lock, flags);
	}
}

extern struct workqueue_struct *keventd_wq;

int schedule_delayed_work(struct delayed_work *dwork, unsigned long delay)
{
	_KDBG("\n");
	return queue_delayed_work(keventd_wq, dwork, delay);
}



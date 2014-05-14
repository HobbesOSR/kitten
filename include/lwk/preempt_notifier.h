/** \file 
 * 
 * Implementation of preemption notifiers to allow tasks to detect
 * context switches. Ported from Linux. 
 */

#ifndef __LWK_PREEMPT_NOTIFIERS_H__
#define __LWK_PREEMPT_NOTIFIERS_H__

#include <lwk/list.h>

struct task_struct;
struct preempt_notifier;
/**
 * preempt_ops - notifiers called when a task is preempted and rescheduled
 * @sched_in: we're about to be rescheduled:
 *    notifier: struct preempt_notifier for the task being scheduled
 *    cpu: cpu we're scheduled on
 * @sched_out: we've just been preempted
 *    notifier: struct preempt_notifier for the task being preempted
 *    next: task that is preempting us
 */

struct preempt_ops {
	void (*sched_in)(struct preempt_notifier * pn, int cpu);
	void (*sched_out)(struct preempt_notifier * pn, struct task_struct * next);
};



/**
 * preempt_notifier - key for installing preemption notifiers
 * @link: internal use
 * @ops: defines the notifier functions to be called
 */
struct preempt_notifier {
	struct hlist_node link;
	struct preempt_ops * ops;
};


void preempt_notifier_register(struct preempt_notifier * notifier);
void preempt_notifier_unregister(struct preempt_notifier * notifier);

static inline void preempt_notifier_init(struct preempt_notifier * notifier, 
					 struct preempt_ops * ops) 
{
	INIT_HLIST_NODE(&notifier->link);
	notifier->ops = ops;
}




#endif

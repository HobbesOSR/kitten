/* The InfiniBand stack does not appear to need notifiers, so make these NOPS */

#include <linux/notifier.h>
#include <linux/rculist.h>

ATOMIC_NOTIFIER_HEAD(panic_notifier_list);

int
blocking_notifier_call_chain(struct blocking_notifier_head *nh, unsigned long val, void *v)
{
	return 0;
}

int
blocking_notifier_chain_register(struct blocking_notifier_head *nh, struct notifier_block *nb)
{
	return 0;
}

int
blocking_notifier_chain_unregister(struct blocking_notifier_head *nh, struct notifier_block *nb)
{
	return 0;
}


/*
 *	Notifier chain core routines.  The exported routines below
 *	are layered on top of these, with appropriate locking added.
 */

static int notifier_chain_register(struct notifier_block **nl,
		struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	rcu_assign_pointer(*nl, n);
	return 0;
}

static int notifier_chain_unregister(struct notifier_block **nl,
		struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n) {
			rcu_assign_pointer(*nl, n->next);
			return 0;
		}
		nl = &((*nl)->next);
	}
	return -ENOENT;
}

/**
 *	atomic_notifier_chain_register - Add notifier to an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to an atomic notifier chain.
 *
 *	Currently always returns zero.
 */
int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
		struct notifier_block *n)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&nh->lock, flags);
	ret = notifier_chain_register(&nh->head, n);
	spin_unlock_irqrestore(&nh->lock, flags);
	return ret;
}

/**
 *	atomic_notifier_chain_unregister - Remove notifier from an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@n: Entry to remove from notifier chain
 *
 *	Removes a notifier from an atomic notifier chain.
 *
 *	Returns zero on success or %-ENOENT on failure.
 */
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
		struct notifier_block *n)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&nh->lock, flags);
	ret = notifier_chain_unregister(&nh->head, n);
	spin_unlock_irqrestore(&nh->lock, flags);
	synchronize_rcu();
	return ret;
}

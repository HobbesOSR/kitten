/* The InfiniBand stack does not appear to need notifiers, so make these NOPS */

#include <linux/notifier.h>

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

#ifndef _LINUX_NOTIFIER_H
#define _LINUX_NOTIFIER_H

struct notifier_block {
	int (*notifier_call)(struct notifier_block *, unsigned long, void *);
};

struct blocking_notifier_head { };

#define BLOCKING_INIT_NOTIFIER_HEAD(name)

extern int blocking_notifier_call_chain(struct blocking_notifier_head *nh, unsigned long val, void *v);
extern int blocking_notifier_chain_register(struct blocking_notifier_head *nh, struct notifier_block *nb);
extern int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh, struct notifier_block *nb);

#endif

#ifndef _LINUX_NOTIFIER_H
#define _LINUX_NOTIFIER_H

struct notifier_block {
	int (*notifier_call)(struct notifier_block *, unsigned long, void *);
};

struct blocking_notifier_head { };

#define BLOCKING_INIT_NOTIFIER_HEAD(name)

#define NETDEV_BONDING_FAILOVER 0x000C

#define NOTIFY_DONE     0x0000      /* Don't care */

extern int blocking_notifier_call_chain(struct blocking_notifier_head *nh, unsigned long val, void *v);
extern int blocking_notifier_chain_register(struct blocking_notifier_head *nh, struct notifier_block *nb);
extern int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh, struct notifier_block *nb);

#endif

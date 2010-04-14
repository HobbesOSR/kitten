#ifndef _LINUX_NOTIFIER_H
#define _LINUX_NOTIFIER_H
#include <linux/spinlock.h>
struct notifier_block {
	int (*notifier_call)(struct notifier_block *, unsigned long, void *);
	struct notifier_block *next;
	int priority;
};

struct blocking_notifier_head { };

#define BLOCKING_INIT_NOTIFIER_HEAD(name)

#define NETDEV_BONDING_FAILOVER 0x000C

#define NOTIFY_DONE     0x0000      /* Don't care */

struct atomic_notifier_head {
	spinlock_t lock;
	struct notifier_block *head;
};

#define ATOMIC_NOTIFIER_INIT(name) {                            \
		.lock = __SPIN_LOCK_UNLOCKED(name.lock),        \
		.head = NULL }
#define ATOMIC_NOTIFIER_HEAD(name)                              \
	struct atomic_notifier_head name =                      \
		ATOMIC_NOTIFIER_INIT(name)


extern int blocking_notifier_call_chain(struct blocking_notifier_head *nh, unsigned long val, void *v);
extern int blocking_notifier_chain_register(struct blocking_notifier_head *nh, struct notifier_block *nb);
extern int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh, struct notifier_block *nb);
extern int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
					struct notifier_block *nb);
extern int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
					struct notifier_block *nb);
#endif

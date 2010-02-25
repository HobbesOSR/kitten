#ifndef _NET_NETEVENT_H
#define _NET_NETEVENT_H

#include <linux/kernel.h>

enum netevent_notif_type {
	NETEVENT_NEIGH_UPDATE = 1, /* arg is struct neighbour ptr */
	NETEVENT_PMTU_UPDATE,      /* arg is struct dst_entry ptr */
	NETEVENT_REDIRECT,     /* arg is struct netevent_redirect ptr */
};

struct notifier_bock;

extern int register_netevent_notifier(struct notifier_block *nb)
{
	LINUX_DBG(FALSE,"\n");
	return 0;
}
extern int unregister_netevent_notifier(struct notifier_block *nb)
{
	LINUX_DBG(TRUE,"\n");
	return 0;
}
extern int call_netevent_notifiers(unsigned long val, void *v)
{
	LINUX_DBG(TRUE,"\n");
	return 0;
}

#endif

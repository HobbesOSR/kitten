/** \file
 * Network device structure.
 */

#ifndef _lwk_netdev_h_
#define _lwk_netdev_h_

#include <lwk/list.h>


/** Network device structure.
 *
 * Each network device in the system must create one of these and
 * pass it to ::netdev_register().
 */
struct netdev
{
	char			name[64];

	int (*send)(
		struct netdev *,
		void *,
		size_t
	);

	struct list_head	next;
};


/**
 * Registers a new network device
 */
extern void
netdev_register(
	struct netdev *		dev
);


/**
 * Initializes the network subsystem; called once at boot.
 */
extern void
netdev_init(void);

#endif

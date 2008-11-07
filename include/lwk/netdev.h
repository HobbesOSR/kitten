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

	uint32_t		ip;
	void *			priv;

	int (*tx)(
		struct netdev *,
		void *,
		size_t
	);

	struct list_head	next;
};


/** Maximum size of a network packet */
#define NETDEV_MTU	((size_t) 8192)

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


/** Receive a packet from the network.
 *
 * Called by low-level interrupt handler to pass an incoming
 * packet into the Kitten network stack.  Typically data will
 * point to the start of an IP header after the network
 * device has stripped off any of its own headers.
 */
extern void
netdev_rx(
	uint8_t *		data,
	size_t			len
);


/** Transmit a packet to the network.
 *
 * Called by the kernel to send a packet.
 *
 * \note Currently depends on IP headers.
 */
extern void
netdev_tx(
	uint32_t		dest,
	uint32_t		src,
	uint8_t			protocol,
	const void *		data,
	size_t			len
);


#endif

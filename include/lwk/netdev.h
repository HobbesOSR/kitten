#ifndef _LWK_NETDEV_H
#define _LWK_NETDEV_H

/**
 * Initializes the network subsystem; called once at boot.
 *
 * This will make the calls to bring up the lwip network
 * library, then initialize all of the network devices that
 * are named on the kernel command line.
 */
extern void netdev_init(void);

#endif

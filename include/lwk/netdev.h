/** \file
 * Network setup.
 */

#ifndef _lwk_netdev_h_
#define _lwk_netdev_h_

/**
 * Initializes the network subsystem; called once at boot.
 *
 * This will make the calls to bring up the lwip network
 * library, then initialize all of the network devices that
 * are named on the kernel command line.
 */
extern void
netdev_init(void);

#endif

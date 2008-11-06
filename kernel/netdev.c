#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/netdev.h>

/**
 * List of all registered network devices in the system.
 */
static LIST_HEAD(netdev_list);

/**
 * Holds a comma separated list of network devices to configure.
 */
static char netdev_str[128];
param_string(net, netdev_str, sizeof(netdev_str));

/**
 * Registers a new network device
 */
void
netdev_register(
	struct netdev *		dev
)
{
	list_add( &dev->next, &netdev_list );
}


/**
 * Initializes the network subsystem; called once at boot.
 */
void
netdev_init(void)
{
	printk( KERN_INFO "%s: Bringing up network devices: '%s'\n", __func__, netdev_str );
	driver_init_list( "net", netdev_str );
}


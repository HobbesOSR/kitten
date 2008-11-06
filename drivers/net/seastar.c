/** \file
 * Interface to the datagram protocol for the XT3 Seastar.
 */

//#include <net/seastar.h>
#include <lwk/driver.h>

void
seastar_init( void )
{
	printk( "******** %s: Looking for seastar\n", __func__ );
}


driver_init( "net", seastar_init );

#include <lwk/kernel.h>

pid_t
sys_fork( void )
{
	printk( "%s: Attempting to fork!\n", __func__ );
	return -ENOSYS;
}


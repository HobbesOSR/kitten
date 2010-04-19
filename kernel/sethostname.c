#include <lwk/kernel.h>
#include <lwk/utsname.h>
#include <arch/uaccess.h>

int
sys_sethostname( char* __user name, size_t len )
{
	if ( len > __UTS_LEN ) return -EINVAL;

	if (copy_from_user( &linux_utsname.nodename, name, len ) )
		return -EFAULT;

	return 0;
}

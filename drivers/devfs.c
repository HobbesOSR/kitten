/** \file
 * Device drivers for common /dev files
 */
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <arch/uaccess.h>


/** Write a user string to the console */
static ssize_t
console_write(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	char			kbuf[ 512 ];
	size_t			klen = len;
	if( klen > sizeof(kbuf)-1 )
		klen = sizeof(kbuf)-1;

	if( copy_from_user( kbuf, (void*) buf, klen ) )
		return -EFAULT;

	kbuf[ klen ] = '\0';

	printk( KERN_USERMSG
		"(%s) %s%s",
		current->name,
		kbuf,
		klen != len ? "<TRUNC>" : ""
	);

	return klen;
}


static struct kfs_fops
console_fops = {
	.write		= console_write
};


/** Do nothing */
static ssize_t
dev_null_rw(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	return 0;
}


static struct kfs_fops
dev_null_fops = {
	.read		= dev_null_rw,
	.write		= dev_null_rw,
};


/** Return a bunch of zeros.
 * \todo validate buffer address!
 */
static ssize_t
dev_zero_read(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	memset( (void*) buf, 0, len );
	return len;
}


static struct kfs_fops
dev_zero_fops = {
	.read		= dev_zero_read,
	.write		= dev_null_rw, // do not acccept any writes
};



#include <lwk/kobject.h>


int
devfs_init(void)
{
	kfs_create( "/dev/console", &console_fops, 0666, 0, 0 );
	kfs_create( "/dev/null", &dev_null_fops, 0666, 0, 0 );
	kfs_create( "/dev/zero", &dev_zero_fops, 0666, 0, 0 );

	struct kobject * kobj = kobject_create();

	return 0;
}


DRIVER_INIT( "kfs", devfs_init );


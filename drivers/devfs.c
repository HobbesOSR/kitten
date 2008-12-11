/** \file
 * Device drivers for common /dev files
 */
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <arch/uaccess.h>
#include <lwk/kobject.h>


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



/*
 * sysfs interface to device files.
 */
static ssize_t
devfs_attr_show(
	struct kobject *kobj,
	struct attribute *attr,
	char *buf
)
{
	printk( "%s: showing\n", __func__ );
	return -1;
}

static ssize_t
devfs_attr_store(
	struct kobject *kobj,
	struct attribute *attr,
	const char *buf,
	size_t len
)
{
	printk( "%s: storing\n", __func__ );
	return -1;
}

static struct sysfs_ops devfs_sysfs_ops = {
	.show		= devfs_attr_show,
	.store		= devfs_attr_store,
};

static struct kobj_type devfs_ktype = {
	.sysfs_ops	= &devfs_sysfs_ops,
};

decl_subsys( devfs, &devfs_ktype, NULL );

struct kobject rootfs;
static struct subsystem sys;
static struct kset sys_devices;

void
devfs_init(void)
{
	kfs_create( "/dev/console", &console_fops, 0666, 0, 0 );
	kfs_create( "/dev/null", &dev_null_fops, 0666, 0, 0 );
	kfs_create( "/dev/zero", &dev_zero_fops, 0666, 0, 0 );

	subsystem_register( &devfs_subsys );

	rootfs.kset = &devfs_subsys.kset;
	kobject_set_name( &rootfs, "sys" );
	kobject_register( &rootfs );

	struct kobject * devfs = kobject_add_dir( &rootfs, "dev" );
	static struct kobject console;
	console.parent = devfs;
	kobject_set_name( &console, "console" );
	kobject_register( &console );
}


driver_init( "kfs", devfs_init );


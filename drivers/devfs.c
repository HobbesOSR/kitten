/** \file
 * Device drivers for common /dev files
 */

/*
 * Console read functionality added by:
 * Jedidiah R. McClurg
 * Northwestern University
 * 4-18-2012
 */

#include <lwk/kfs.h>
#include <lwk/console.h>
#include <lwk/driver.h>
#include <lwk/dev.h>
#include <lwk/stat.h>
#include <lwk/spinlock.h>
#include <lwk/waitq.h>
#include <lwk/sched.h>
#include <arch/uaccess.h>


/**
 * Writes a string to the console.
 */
static ssize_t
devfs_console_write(
	struct file *	file,
	const char *	buf,
	size_t		len,
	loff_t *	off
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


/**
 * Reads a string from the console.
 */
static ssize_t
devfs_console_read(
	struct file *	file,
	char *		buf,
	size_t		len,
	loff_t *	off
)
{
	char tmp[512];
	ssize_t n;

	n = console_inbuf_read(tmp, sizeof(tmp));
	if (n > 0) {
		// Copy the characters to userspace.
		if (copy_to_user(buf, tmp, n))
			return -EFAULT;
	}

	return n;
}


static struct kfs_fops
console_fops = {
	.write		= devfs_console_write,
	.read		= devfs_console_read,
};


/** Do nothing */
static ssize_t
dev_null_r(
	struct file *	file,
	char *  buf,
	size_t		len,
	loff_t *	off
)
{
	return 0;
}

static ssize_t
dev_null_w(
	struct file *	file,
	const char *  buf,
	size_t		len,
	loff_t *	off
)
{
	return 0;
}


static struct kfs_fops
dev_null_fops = {
	.read		= dev_null_r,
	.write		= dev_null_w,
};


/** Return a bunch of zeros.
 * \todo validate buffer address!
 */
static ssize_t
dev_zero_read(
	struct file *	file,
	char *		buf,
	size_t		len,
	loff_t *	off

)
{
	memset( (void*) buf, 0, len );
	return len;
}


static struct kfs_fops
dev_zero_fops = {
	.read		= dev_zero_read,
	.write		= dev_null_w, // do not acccept any writes
};


#ifdef CONFIG_LINUX
extern struct kfs_fops def_chr_fops;


void create_dev(char * path, int major, int minor) {
	struct inode * inode;
	//printk("mknod: %s - %d:%d\n", path, major, minor);

	inode = kfs_create( path, NULL, &def_chr_fops, 0777, 0, 0 );
	inode->i_rdev = MKDEV(major, minor);
}
#endif

int
devfs_init(void)
{
	struct inode *inode;

	inode = kfs_create("/dev/console", NULL, &console_fops, S_IFCHR|0666, NULL, 0);
	if (!inode)
		panic("Failed to create /dev/console.");

	/* Glibc depends on /dev/console being major 136 */
	inode->i_rdev = MKDEV(136, 0);

	inode = kfs_create("/dev/null", NULL, &dev_null_fops, S_IFCHR|0666, NULL, 0);
	if (!inode)
		panic("Failed to create /dev/null.");

	inode = kfs_create("/dev/zero", NULL, &dev_zero_fops, S_IFCHR|0666, NULL, 0);
	if (!inode)
		panic("Failed to create /dev/zero.");

	return 0;
}


DRIVER_INIT( "kfs", devfs_init );

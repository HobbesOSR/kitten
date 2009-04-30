/** \file
 * sysfs wrappers around KFS objects.
 */

#include <lwk/kfs.h>
#include <lwk/print.h>
#include <lwk/driver.h>
#include <lwk/kobject.h>
#include <lwk/sysfs.h>

struct kfs_file * sysfs_root;


void
sysfs_init( void )
{
	sysfs_root = kfs_create(
		"/sys",
		&kfs_default_fops,
		0777,
		0,
		0
	);

	printk( "%s: root=%p\n", __func__, sysfs_root );
}

driver_init( "kfs",  sysfs_init );


static struct kfs_fops kobject_fops = {
};


/**
 *	sysfs_create_dir - create a directory for an object.
 *	@kobj:		object we're creating directory for. 
 */
int sysfs_create_dir(struct kobject * kobj)
{
	struct sysfs_dirent *parent_sd, *sd;
	int error = 0;

	BUG_ON(!kobj);

	if (kobj->parent)
		parent_sd = kobj->parent->sd;
	else
		parent_sd = sysfs_root;

#ifdef __LWK__
	kobj->sd = kfs_mkdirent(
		parent_sd,
		kobject_name(kobj),
		&kfs_default_fops,
		0777,
		0,
		0
	);
	return kobj->sd == NULL;
#else
	error = create_dir(kobj, parent_sd, kobject_name(kobj), &sd);
	if (!error)
		kobj->sd = sd;
	return error;
#endif
}

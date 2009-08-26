#include <lwk/kfs.h>
#include <lwk/print.h>
#include <lwk/driver.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

struct kfs_file *sysfs_root;


/**
 * sysfs_create_dir - create a directory for an object.
 * @kobj:    object we're creating directory for. 
 */
int
sysfs_create_dir(
	struct kobject *		kobj
)
{
	struct sysfs_dirent *parent;

	BUG_ON(!kobj);

	if (kobj->parent)
		parent = kobj->parent->sd;
	else
		parent = sysfs_root;

	kobj->sd = kfs_mkdirent(
		parent,
		kobject_name(kobj),
		&kfs_default_fops,
		0777,
		NULL,
		0
	);

	return (kobj->sd == NULL);
}


/**
 * sysfs_create_file - create an attribute file for an object.
 * @kobj:    object we're creating for.
 * @attr:    attribute descriptor.
 */
int
sysfs_create_file(
	struct kobject *		kobj,
	const struct attribute *	attr
)
{
	BUG_ON(!kobj || !kobj->sd || !attr);

	/* TODO: Actually implement this! */
	printk(KERN_WARNING "%s needs to be implemented.\n", __FUNCTION__);

	return 0;
}


/**
 * sysfs_create_link - create symlink between two objects.
 * @kobj:    object whose directory we're creating the link in.
 * @target:  object we're pointing to.
 * @name:    name of the symlink.
 */
int
sysfs_create_link(
	struct kobject *		kobj,
	struct kobject *		target,
	const char *			name
)
{
	BUG_ON(!name);

	/* TODO: Actually implement this! */
	printk(KERN_WARNING "%s needs to be implemented.\n", __FUNCTION__);

	return 0;
}


/**
 * sysfs_create_bin_file - create binary file for object.
 * @kobj:    object.
 * @attr:    attribute descriptor.
 */
int
sysfs_create_bin_file(
	struct kobject *		kobj,
	struct bin_attribute *		attr
)
{
	BUG_ON(!kobj || !kobj->sd || !attr);

	/* TODO: Actually implement this! */
	printk(KERN_WARNING "%s needs to be implemented.\n", __FUNCTION__);

	return 0;
}


/**
 * sysfs_create_group - given a directory kobject, create an attribute group
 * @kobj:    The kobject to create the group on
 * @grp:     The attribute group to create
 *
 * This function creates a group for the first time.  It will explicitly
 * warn and error if any of the attribute files being created already exist.
 *
 * Returns 0 on success or error.
 */
int
sysfs_create_group(
	struct kobject *		kobj,
	const struct attribute_group *	grp
)
{
	BUG_ON(!kobj || !kobj->sd);

	/* TODO: Actually implement this! */
	printk(KERN_WARNING "%s needs to be implemented.\n", __FUNCTION__);

	return 0;
}


int
sysfs_init(void)
{
	sysfs_root = kfs_create(
		"/sys",
		&kfs_default_fops,
		0777,
		0,
		0
	);

	return 0;
}

DRIVER_INIT("kfs",  sysfs_init);

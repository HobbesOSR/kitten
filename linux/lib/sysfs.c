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

	//printk(KERN_WARNING "%s creating dir %s/%s\n", __FUNCTION__,parent->name,kobject_name(kobj));
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


struct sysfs_buffer {
	struct sysfs_ops        * ops;
	struct kobject          * kobj;
	struct attribute        * attr;
};

static ssize_t
sysfs_read_file(struct kfs_file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[1024];
	char *bufp = (char *) &buffer;
	struct sysfs_buffer * sbuffer = file->private_data;
	struct sysfs_ops * ops = sbuffer->ops;
	struct kobject * kobj = sbuffer->kobj;
	struct attribute * attr = sbuffer->attr;
	loff_t pos = 0; // = *ppos;
	size_t ret;

	count = ops->show(kobj, attr, bufp);
	if (pos < 0)
		return -EINVAL;
	if (!count)
		return 0;
	ret = copy_to_user(buf, bufp + pos, count);
	if (ret == count)
		return -EFAULT;
	count -= ret;
	//*ppos = pos + count;
	return count;
}

static int sysfs_open_file(struct inode *inode, struct kfs_file *file)
{

	if (file->private_data != NULL)
		return 0;
	else
		return -ENODEV;


}


const struct kfs_fops sysfs_file_operations = {
	.read = sysfs_read_file,
	.open = sysfs_open_file,
};


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


	struct sysfs_buffer *buffer;
	struct kfs_file *file;

	/* TODO: Actually implement this! */
	//printk(KERN_WARNING "%s creating %s %s\n", __FUNCTION__,kobject_name(kobj), attr->name);
	file = kfs_mkdirent(
		kobj->sd,
		attr->name,
		&sysfs_file_operations,
		attr->mode, // should be 0444 as we don't support writes..
		NULL,
		0
	);

	buffer = kzalloc(sizeof(struct sysfs_buffer), GFP_KERNEL);
	buffer->kobj = kobj;
	if (buffer->kobj->ktype && buffer->kobj->ktype->sysfs_ops) {
		buffer->ops = buffer->kobj->ktype->sysfs_ops;
	} else {
		WARN(1, KERN_ERR "missing sysfs attribute operations for "
			"kobject: %s\n", kobject_name(kobj));
        }

	buffer->attr = (struct attribute *) attr;
	file->private_data = buffer;
	//printk(KERN_WARNING "%s needs to be implemented.\n", __FUNCTION__);

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
	//printk(KERN_WARNING "%s needs to be implemented.\n", __FUNCTION__);
	//printk(KERN_WARNING "sysfs_create_link %s -> %s/%s\n", kobject_name(kobj),
	//					kobject_name(target), name);

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
	//printk(KERN_WARNING "%s needs to be implemented.\n", __FUNCTION__);

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
	//printk(KERN_WARNING "%s needs to be implemented.\n", __FUNCTION__);

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

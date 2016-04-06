#include <lwk/kobject.h>
#include <lwk/sysfs.h>
#include <lwk/stat.h>
#include <lwk/kfs.h>

#include <arch/uaccess.h>

struct inode * sysfs_root = NULL;




/**
 * sysfs_create_dir - create a directory for an object.
 * @kobj:    object we're creating directory for.
 */
int
sysfs_create_dir(struct kobject * kobj)
{
	struct sysfs_dirent * parent = NULL;

	BUG_ON(!kobj);

	if (kobj->parent) {
		parent = kobj->parent->sd;
	} else {
		parent = sysfs_root;
	}

	//printk(KERN_WARNING "%s creating dir %s/%s\n", __FUNCTION__,parent->name,kobject_name(kobj));
	kobj->sd = kfs_mkdirent(parent,
				kobject_name(kobj),
				NULL,
				&kfs_default_fops,
				0777 | S_IFDIR,
				NULL,
				0);

	return (kobj->sd == NULL);
}

struct sysfs_buffer {
	const struct sysfs_ops  * ops;
	struct kobject          * kobj;
        struct attribute        * attr;
};

static ssize_t
sysfs_read_file(struct file * file, 
		char __user * buf, 
		size_t        count, 
		loff_t      * ppos)
{
	char buffer[1024];
	char * bufp = (char *)&buffer;

	struct sysfs_buffer    * sbuffer = file->inode->priv;
	const struct sysfs_ops * ops     = sbuffer->ops;
	struct kobject         * kobj    = sbuffer->kobj;
	struct attribute       * attr    = sbuffer->attr;

	loff_t pos = 0; // = *ppos;
	size_t ret;

	count = ops->show(kobj, attr, bufp);

	if (pos < 0) {
		return -EINVAL;
	}

	if (!count) {
		return 0;
	}

	ret = copy_to_user(buf, bufp + pos, count);

	if (ret == count) {
		return -EFAULT;
	}

	count -= ret;
	//*ppos = pos + count;

	return count;
}

static int 
sysfs_open_file(struct inode * inode, 
		struct file  * file)
{
	if (file->inode->priv != NULL) {
		return 0;
	} else {
		return -ENODEV;
	}
}


const struct kfs_fops sysfs_file_operations = {
	.read    = sysfs_read_file,
	.open    = sysfs_open_file,
	.readdir = kfs_readdir,
};

int
sysfs_create_file(struct kobject         * kobj,
		  const struct attribute * attr)
{
	struct sysfs_buffer * buffer = NULL;
	struct inode        * inode  = NULL;

	BUG_ON(!kobj);
	BUG_ON(!kobj->sd);
	BUG_ON(!attr);


	/* TODO: Actually implement this! */
	//printk(KERN_WARNING "%s creating %s %s\n", __FUNCTION__,kobject_name(kobj), attr->name);

	buffer = kmem_alloc(sizeof(struct sysfs_buffer));

	if (buffer == NULL) {
	    return -1;
	}

	buffer->kobj = kobj;

	if ( (buffer->kobj->ktype) && 
	     (buffer->kobj->ktype->sysfs_ops) )
	{
		buffer->ops = buffer->kobj->ktype->sysfs_ops;
	} else {
		WARN(1, KERN_ERR "missing sysfs attribute operations for "
		     "kobject: %s\n", kobject_name(kobj));
	}
	
	buffer->attr = (struct attribute *) attr;

	inode = kfs_mkdirent(kobj->sd,
			     attr->name,
			     NULL,
			     &sysfs_file_operations,
			     attr->mode & ~S_IFMT, // should be 0444 as we don't support writes..
			     buffer,
			     sizeof(struct sysfs_buffer));

	if (NULL == inode) {
		kmem_free(buffer);
		return -1;
	}

	/* TODO: silently forgetting this inode is not cool; we will have
	   to destroy it, eventually (or will we do this recursively by
	   destroyxing the parent inode?) ... */

	return 0;

	return (0);
}

void
sysfs_remove_file(struct kobject         * kobj, 
		  const struct attribute * attr)
{
	printk( "***** %s: not implemented\n", __func__ );
	return;
}

void
sysfs_remove_group(struct kobject               * kobj,
		   const struct attribute_group * grp)
{
	printk( "***** %s: not implemented\n", __func__ );
	return;
}

void
sysfs_remove_dir(struct kobject * kobj)
{
	printk( "***** %s: not implemented\n", __func__ );
	return;
}

int
sysfs_create_group(struct kobject               * kobj, 
		   const struct attribute_group * grp)
{
	struct attribute * const * attr;

	struct sysfs_buffer * buffer;
	struct inode        * dir_sd;
	struct inode        * inode;

	int error = 0;
	int i;

	BUG_ON(!kobj);
	BUG_ON(!kobj->sd);

	if (grp->name) {
		dir_sd = kfs_mkdirent(kobj->sd,
				      grp->name,
				      NULL,
				      &kfs_default_fops,
				      0777 | S_IFDIR,
				      NULL,
				      0);
		if (error) {
			return error;
		}
	} else {
		dir_sd = kobj->sd;
	}

	for (i = 0, attr = grp->attrs; (*attr) && (!error); i++, attr++) {
		mode_t mode = 0;

		if (grp->is_visible) {
			mode = grp->is_visible(kobj, *attr, i);
			if (!mode) continue;
		}

		buffer = kmem_alloc(sizeof(struct sysfs_buffer));

		if (buffer == NULL) {
			error = -1;
			break;
		}

		buffer->kobj = kobj;

		if ( (buffer->kobj->ktype) && 
		     (buffer->kobj->ktype->sysfs_ops) )
		{
			buffer->ops = buffer->kobj->ktype->sysfs_ops;
		} else {
			WARN(1,KERN_ERR "missing sysfs attribute operations for"
				"kobject: %s\n", kobject_name(kobj));
		}

		buffer->attr = (struct attribute *) (*attr);

		inode = kfs_mkdirent(dir_sd,
				     (*attr)->name,
				     NULL,
				     &sysfs_file_operations,
				     ((*attr)->mode | mode) & ~S_IFMT,
				     buffer,
				     sizeof(struct sysfs_buffer));

		if (inode == NULL) {
			kmem_free(buffer);
			error =  -1;
			break;
		}

		error = 0;
	}

	return error;
}

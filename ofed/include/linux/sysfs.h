/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LINUX_SYSFS_H_
#define	_LINUX_SYSFS_H_

#include <lwk/kfs.h>

struct attribute {
	const char 	*name;
	struct module	*owner;
	mode_t		mode;
};

struct sysfs_ops {
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *, const char *,
	    size_t);
};

struct attribute_group {
	const char		*name;
	mode_t                  (*is_visible)(struct kobject *,
				    struct attribute *, int);
	struct attribute	**attrs;
};

#define	__ATTR(_name, _mode, _show, _store) {				\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
        .show = _show, .store  = _store,				\
}

#define	__ATTR_RO(_name) {						\
	.attr = { .name = __stringify(_name), .mode = 0444 },		\
	.show   = _name##_show,						\
}

#define	__ATTR_NULL	{ .attr = { .name = NULL } }





extern struct inode *sysfs_root;




/**
 * sysfs_create_dir - create a directory for an object.
 * @kobj:    object we're creating directory for.
 */
static int
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
		NULL,
		&kfs_default_fops,
		0777 | S_IFDIR,
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

static  ssize_t
sysfs_read_file(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[1024];
	char *bufp = (char *) &buffer;
	struct sysfs_buffer * sbuffer = file->inode->priv;
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

static int sysfs_open_file(struct inode *inode, struct file *file)
{
	if (file->inode->priv != NULL)
		return 0;
	else
		return -ENODEV;


}


const struct kfs_fops sysfs_file_operations = {
	.read = sysfs_read_file,
	.open = sysfs_open_file,
	.readdir = kfs_readdir,
};

static inline int
sysfs_create_file(struct kobject *kobj, const struct attribute *attr)
{
	BUG_ON(!kobj || !kobj->sd || !attr);


	struct sysfs_buffer *buffer;
	struct inode *inode;

	/* TODO: Actually implement this! */
	//printk(KERN_WARNING "%s creating %s %s\n", __FUNCTION__,kobject_name(kobj), attr->name);

	buffer = kzalloc(sizeof(struct sysfs_buffer), GFP_KERNEL);
	if(NULL == buffer)
	  return -1;
	buffer->kobj = kobj;
	if (buffer->kobj->ktype && buffer->kobj->ktype->sysfs_ops) {
		buffer->ops = buffer->kobj->ktype->sysfs_ops;
	} else {
		WARN(1, KERN_ERR "missing sysfs attribute operations for "
			"kobject: %s\n", kobject_name(kobj));
	}

	buffer->attr = (struct attribute *) attr;

	inode = kfs_mkdirent(
		kobj->sd,
		attr->name,
		NULL,
		&sysfs_file_operations,
		attr->mode & ~S_IFMT, // should be 0444 as we don't support writes..
		buffer,
		sizeof(struct sysfs_buffer)
	);
	if(NULL == inode) {
	  kfree(buffer);
	  return -1;
	}
	/* TODO: silently forgetting this inode is not cool; we will have
	   to destroy it, eventually (or will we do this recursively by
	   destroyxing the parent inode?) ... */

	return 0;

	return (0);
}

static inline void
sysfs_remove_file(struct kobject *kobj, const struct attribute *attr)
{
	printk( "***** %s: not implemented\n", __func__ );
	return;
}

static inline void
sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp)
{
	printk( "***** %s: not implemented\n", __func__ );
	return;
}

static inline void
sysfs_remove_dir(struct kobject *kobj)
{
	printk( "***** %s: not implemented\n", __func__ );
	return;
}

static inline int
sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp)
{
	BUG_ON(!kobj || !kobj->sd);

	struct inode *dir_sd;
	int error = 0, i;
	struct attribute *const* attr;

	struct sysfs_buffer *buffer;
	struct inode *inode;

	if (grp->name) {
		dir_sd = kfs_mkdirent(
			kobj->sd,
			grp->name,
			NULL,
			&kfs_default_fops,
			0777 | S_IFDIR,
			NULL,
			0
			);
		if (error)
			return error;
	} else
		dir_sd = kobj->sd;

	for (i = 0, attr = grp->attrs; *attr && !error; i++, attr++) {
		mode_t mode = 0;
		if (grp->is_visible) {
			mode = grp->is_visible(kobj, *attr, i);
			if (!mode)
				continue;
		}

		buffer = kzalloc(sizeof(struct sysfs_buffer), GFP_KERNEL);
		if(NULL == buffer) {
			error = -1;
			break;
		}
		buffer->kobj = kobj;
		if (buffer->kobj->ktype && buffer->kobj->ktype->sysfs_ops) {
			buffer->ops = buffer->kobj->ktype->sysfs_ops;
		} else {
			WARN(1,KERN_ERR "missing sysfs attribute operations for"
				"kobject: %s\n", kobject_name(kobj));
		}
		buffer->attr = (struct attribute *) (*attr);

		inode = kfs_mkdirent(
			dir_sd,
			(*attr)->name,
			NULL,
			&sysfs_file_operations,
			((*attr)->mode | mode) & ~S_IFMT,
			buffer,
			sizeof(struct sysfs_buffer)
			);
		if(NULL == inode) {
			kfree(buffer);
			error =  -1;
			break;
		}
		error = 0;
	}

	return error;
}





#define sysfs_attr_init(attr) do {} while(0)

#endif	/* _LINUX_SYSFS_H_ */

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



#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cdev.h>
//#include <linux/file.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/vmalloc.h>

#include <lwk/kfs.h>
#include <lwk/driver.h>


static struct kobject *class_kobj;
static struct kobject *devices_kobj;
static struct kobject *dev_kobj;
/*
struct kobject *sysfs_dev_char_kobj;
struct kobject *sysfs_dev_block_kobj;
*/

extern struct inode *sysfs_root;

#include <linux/rbtree.h>

/* From sys/queue.h */
struct name {								\
	struct type *lh_first;	/* first element */			\
};

struct kobject class_root;
struct device linux_rootdev;

struct class miscclass;

/*
struct list_head pci_drivers;
struct list_head pci_devices;
spinlock_t pci_lock;
*/

struct kobj_map *cdev_map;


/*
int
panic_cmp(struct rb_node *one, struct rb_node *two)
{
	panic("no cmp");
}



RB_GENERATE(linux_root, rb_node, __entry, panic_cmp);
 
*/

int
kobject_set_name(struct kobject *kobj, const char *fmt, ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);

	return (error);
}

static inline int
kobject_add_complete(struct kobject *kobj, struct kobject *parent)
{
	struct kobj_type *t;
	int error;

	kobj->parent = kobject_get(parent);
	error = sysfs_create_dir(kobj);
	if (error == 0 && kobj->ktype && kobj->ktype->default_attrs) {
		struct attribute **attr;
		t = kobj->ktype;

		for (attr = t->default_attrs; *attr != NULL; attr++) {
			error = sysfs_create_file(kobj, *attr);
			if (error)
				break;
		}
		if (error)
			sysfs_remove_dir(kobj);
		
	}
	return (error);
}

int
kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);
	if (error)
		return (error);

	return kobject_add_complete(kobj, parent);
}

void
kobject_release(struct kref *kref)
{
	struct kobject *kobj;
	char *name;

	kobj = container_of(kref, struct kobject, kref);
	sysfs_remove_dir(kobj);
	if (kobj->parent)
		kobject_put(kobj->parent);
	kobj->parent = NULL;
	name = kobj->name;
	if (kobj->ktype && kobj->ktype->release)
		kobj->ktype->release(kobj);
	kfree(name);
}

static void
kobject_kfree(struct kobject *kobj)
{

	kfree(kobj);
}

struct kobj_type kfree_type = { .release = kobject_kfree };


struct device *
device_create(struct class *class, struct device *parent, dev_t devt,
    void *drvdata, const char *fmt, ...)
{
	struct device *dev;
	va_list args;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	dev->parent = parent;
	dev->class = class;
	dev->devt = devt;
	dev->driver_data = drvdata;
	va_start(args, fmt);
	kobject_set_name_vargs(&dev->kobj, fmt, args);
	va_end(args);

	device_register(dev);

	return (dev);
}

int
kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype,
    struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int error;

	kobject_init(kobj, ktype);
	kobj->ktype = ktype;
	kobj->parent = parent;
	kobj->name = NULL;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);
	if (error)
		return (error);
	return kobject_add_complete(kobj, parent);
}




static ssize_t 
linux_shim_read(struct file * filp, 
		char __user * ubuf, 
		size_t        size,
		loff_t      * off)
{
	struct linux_file * file = filp->private_data;

	return file->fops->read((struct file *)file, ubuf, size, off);	
}

static ssize_t 
linux_shim_write(struct file       * filp, 
		 const char __user * ubuf, 
		 size_t              size,
		 loff_t            * off)
{
	struct linux_file * file = filp->private_data;
    
	return file->fops->write((struct file *)file, ubuf, size, off);	
}

static unsigned int 
linux_shim_poll(struct file              * filp,
		struct poll_table_struct * table) 
{
	struct linux_file * file = filp->private_data;
    
	return file->fops->poll((struct file *)file, table);
}

static long 
linux_shim_unlocked_ioctl(struct file  * filp,
			  unsigned int   ioctl, 
			  unsigned long  arg)
{
	struct linux_file * file = filp->private_data;
    
	return file->fops->unlocked_ioctl((struct file *)file, ioctl, arg);
}

static int 
linux_shim_mmap(struct file           * filp, 
		struct vm_area_struct * vma)
{
	struct linux_file * file = filp->private_data;
    
	return file->fops->mmap((struct file *)file, vma);
}

static int 
linux_shim_open(struct inode * inode, 
		struct file  * filp)
{
	struct linux_file * file = filp->private_data;
    
	return file->fops->open(inode, (struct file *)file);
}
	
static int 
linux_shim_release(struct inode * inode,
		   struct file  * filp)
{
	struct linux_file * file = filp->private_data;
    
	return file->fops->release(inode, (struct file *)file);
}

static int 
linux_shim_fasync(int           fd, 
		  struct file * filp, 
		  int           on)
{
	struct linux_file * file = filp->private_data;
    
	return file->fops->fasync(fd, (struct file *)file, on);
}

struct kfs_fops linux_shim_fops = {
	.open           = linux_shim_open,
	.release        = linux_shim_release,
	.read           = linux_shim_read,
	.write          = linux_shim_write,
	.poll           = linux_shim_poll,
	.unlocked_ioctl = linux_shim_unlocked_ioctl,
	.mmap           = linux_shim_mmap,
	.fasync         = linux_shim_fasync
};








static int 
devices_init(void)
{
	devices_kobj = kobject_create_and_add("devices", NULL);
	if (!devices_kobj)
		return -ENOMEM;
	dev_kobj = kobject_create_and_add("dev", NULL);
	if (!dev_kobj)
		goto dev_kobj_err;

	/*
	sysfs_dev_block_kobj = kobject_create_and_add("block", dev_kobj);
	if (!sysfs_dev_block_kobj)
		goto block_kobj_err;
	sysfs_dev_char_kobj = kobject_create_and_add("char", dev_kobj);
	if (!sysfs_dev_char_kobj)
		goto char_kobj_err;
	
	*/
	return 0;
	
	/*
	  char_kobj_err:
	  kobject_put(sysfs_dev_block_kobj);
	  block_kobj_err:
	  kobject_put(dev_kobj);
	*/
 dev_kobj_err:
	kobject_put(devices_kobj);
	
	return -ENOMEM;
}


static int
linux_compat_init(void)
{
	sysfs_root = kfs_create(
				"/sys",
				NULL,
				&kfs_default_fops,
				0777 | S_IFDIR,
				0,
				0
				);

	/*
	kobject_init(&class_root, &class_ktype);
	kobject_set_name(&class_root, "class");

	class_root.oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(rootoid),
	    OID_AUTO, "class", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "class");
	*/
	class_kobj = kobject_create_and_add("class", NULL);
	if (!class_kobj) {
		return -ENOMEM;
	}

	/*
	  kobject_init(&linux_rootdev.kobj, &dev_ktype);
	  kobject_set_name(&linux_rootdev.kobj, "device");
	*/
	devices_init();
	

	//	linux_rootdev.bsddev = root_bus;

	miscclass.name = "misc";
	class_register(&miscclass);

	/*
	INIT_LIST_HEAD(&pci_drivers);
	INIT_LIST_HEAD(&pci_devices);
	spin_lock_init(&pci_lock);
	*/
	return 0;
}

DRIVER_INIT("linux", linux_compat_init);

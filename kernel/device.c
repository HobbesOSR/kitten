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


#include <lwk/device.h>
#include <lwk/kobject.h>


static struct kobject * class_kobj   = NULL;
static struct kobject * devices_kobj = NULL;
static struct kobject * dev_kobj     = NULL;

static struct kobject * sysfs_dev_char_kobj  = NULL;
static struct kobject * sysfs_dev_block_kobj = NULL;

struct kobject class_root;
struct device  dev_root;




struct device *
get_device(struct device * dev)
{
	if (dev) {
		kobject_get(&dev->kobj);
	}

	return (dev);
}

void
put_device(struct device * dev)
{
	if (dev) {
		kobject_put(&dev->kobj);
	}
}




static void
device_release(struct kobject * kobj)
{
	struct device * dev = NULL;

	dev = container_of(kobj, struct device, kobj);

	/* This is the precedence defined by linux. */
	if (dev->release) {
		dev->release(dev);
	} else if ( (dev->class) && (dev->class->dev_release) ) {
		dev->class->dev_release(dev);
	}
}

static ssize_t
dev_show(struct kobject   * kobj, 
	 struct attribute * attr, 
	 char             * buf)
{
	struct device_attribute * dattr = NULL;
	ssize_t error;

	dattr = container_of(attr, struct device_attribute, attr);
	error = -EIO;

	if (dattr->show) {
		error = dattr->show(container_of(kobj, struct device, kobj),
				    dattr, buf);
	}

	return (error);
}

static ssize_t
dev_store(struct kobject   * kobj, 
	  struct attribute * attr,
	  const char       * buf,
	  size_t             count)
{
	struct device_attribute * dattr = NULL;
	ssize_t error;

	dattr = container_of(attr, struct device_attribute, attr);
	error = -EIO;

	if (dattr->store) {
		error = dattr->store(container_of(kobj, struct device, kobj),
				     dattr, buf, count);
	}

	return (error);
}

static struct sysfs_ops dev_sysfs = { 
	.show  = dev_show, 
	.store = dev_store, 
};

static struct kobj_type dev_ktype = {
	.release   =  device_release,
	.sysfs_ops = &dev_sysfs
};



int 
init_devices(void)
{
	/* Create /sys/class */
	class_kobj = kobject_create_and_add("class", NULL);
	
	if (!class_kobj) {
		return -ENOMEM;
	}

	/* Create /sys/devices */
	devices_kobj = kobject_create_and_add("devices", NULL);

	if (!devices_kobj) {
		return -ENOMEM;
	}

	/* Create /sys/dev */
	dev_kobj = kobject_create_and_add("dev", NULL);

	if (!dev_kobj) {
		goto dev_kobj_err;
	}


	/* This is a hack to deal with the fact that we 
	   do not properly handle sysfs entries for PCI devices */
	kobject_init(&dev_root.kobj, &dev_ktype);
	//	kobject_set_name(&dev_root.kobj, "device");
	kobject_add(&dev_root.kobj, NULL, "device_root");

	sysfs_dev_block_kobj = kobject_create_and_add("block", dev_kobj);

	if (!sysfs_dev_block_kobj) {
		goto block_kobj_err;
	}

	sysfs_dev_char_kobj = kobject_create_and_add("char", dev_kobj);
	if (!sysfs_dev_char_kobj) {
		goto char_kobj_err;
	}


	return 0;
	
 char_kobj_err:
	kobject_put(sysfs_dev_block_kobj);
 block_kobj_err:
	kobject_put(dev_kobj);
 dev_kobj_err:
	kobject_put(devices_kobj);
	
	return -ENOMEM;
}


char *
dev_name(const struct device * dev)
{
 	return kobject_name(&dev->kobj);
}

/*
 * Devices are registered and created for exporting to sysfs.  create
 * implies register and register assumes the device fields have been
 * setup appropriately before being called.
 */
int
device_register(struct device * dev)
{
 
	kobject_init(&dev->kobj, &dev_ktype);

	if (dev->class) {
		kobject_add(&dev->kobj, &dev->class->kobj, dev_name(dev));
	}

	/* Should this happen all the time?  */
	if (dev->parent) {
		kobject_add(&dev->kobj, &dev->parent->kobj, kobject_name(&dev->kobj));
	}


	return (0);
}

void
device_unregister(struct device * dev)
{
	put_device(dev);
}

void
device_init(struct device * dev, 
	    struct class  * class, 
	    struct device * parent, 
	    dev_t           devt,
	    void          * drvdata,
	    const char    * fmt,
	    ...)
{
	va_list args;

	dev->parent      = parent;
	dev->class       = class;
	dev->devt        = devt;
	dev->driver_data = drvdata;

	va_start(args, fmt);
	kobject_set_name_vargs(&dev->kobj, fmt, args);
	va_end(args);


	device_register(dev);

	return;
} 

struct device *
device_create(struct class  * class, 
	      struct device * parent, 
	      dev_t           devt,
	      void          * drvdata,
	      const char    * fmt,
	      ...)
{
	struct device * dev = NULL;
	va_list args;

 	dev = kmem_alloc(sizeof(*dev));

	if (!dev) {
	    printk(KERN_ERR "Could not allocate new device\n");
	    return NULL;
	}

	dev->parent      = parent;
	dev->class       = class;
	dev->devt        = devt;
	dev->driver_data = drvdata;

	va_start(args, fmt);
	kobject_set_name_vargs(&dev->kobj, fmt, args);
	va_end(args);

	device_register(dev);

	return (dev);
}





void
device_destroy(struct class * class,
	       dev_t          devt)
{
    return;
}

static ssize_t
class_show(struct kobject   * kobj,
	   struct attribute * attr,
	   char             * buf)
{
	struct class_attribute * dattr = NULL;
	ssize_t error;

	dattr = container_of(attr, struct class_attribute, attr);
	error = -EIO;

	if (dattr->show) {
		error = dattr->show(container_of(kobj, struct class, kobj),
				    buf);
	}
	return (error);
}

static ssize_t
class_store(struct kobject   * kobj,
	    struct attribute * attr,
	    const char       * buf,
	    size_t             count)
{
	struct class_attribute * dattr = NULL;
	ssize_t error;

	dattr = container_of(attr, struct class_attribute, attr);
	error = -EIO;

	if (dattr->store) {
		error = dattr->store(container_of(kobj, struct class, kobj),
				     buf, count);
	}

	return (error);
}

static void
class_release(struct kobject * kobj)
{
	struct class * class = NULL;

	class = container_of(kobj, struct class, kobj);

	if (class->class_release) {
		class->class_release(class);
	}
}

static struct sysfs_ops class_sysfs = {
	.show  = class_show,
	.store = class_store,
};
static struct kobj_type class_ktype = {
	.release   =  class_release,
	.sysfs_ops = &class_sysfs
};

int
class_register(struct class * class)
{

	kobject_init     (&class->kobj, &class_ktype);
	kobject_set_name (&class->kobj,  class->name);
	kobject_add      (&class->kobj, &class_root, class->name);

	return (0);
}

void
class_unregister(struct class * class)
{
	kobject_put(&class->kobj);
}

static void
class_free(struct class * class)
{
	kmem_free(class);
}

struct class *
class_create(struct module * owner, 
	     const char    * name)
{
	struct class * class = NULL;
	int error;

	class                = kmem_alloc(sizeof(*class));
	class->owner         = owner;
	class->name          = name;
	class->class_release = class_free;
	error                = class_register(class);

	if (error) {
		kmem_free(class);
		return (NULL);
	}

	return (class);
}

void
class_destroy(struct class * class)
{
	if (class == NULL) {
		return;
	}

	class_unregister(class);
}



int
device_create_file(struct device                 * dev,
		   const struct device_attribute * attr)
{
	if (dev) {
		return sysfs_create_file(&dev->kobj, &attr->attr);
	}

	return -EINVAL;
}

void
device_remove_file(struct device                 * dev, 
		   const struct device_attribute * attr)
{
	if (dev) {
		sysfs_remove_file(&dev->kobj, &attr->attr);
	}
}

int
class_create_file(struct class                 * class, 
		  const struct class_attribute * attr)
{
	if (class) {
		return sysfs_create_file(&class->kobj, &attr->attr);
	}

	return -EINVAL;
}


void
class_remove_file(struct class                 * class, 
		  const struct class_attribute * attr)
{

	if (class) {
		sysfs_remove_file(&class->kobj, &attr->attr);
	}
}

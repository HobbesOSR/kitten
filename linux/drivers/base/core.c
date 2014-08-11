/*
 * drivers/base/core.c - core driver model code (device registration, etc)
 *
 * Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 * Copyright (c) 2006 Greg Kroah-Hartman <gregkh@suse.de>
 * Copyright (c) 2006 Novell, Inc.
 *
 * This file is released under the GPLv2
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kdev_t.h>
#include <linux/notifier.h>
#include <linux/genhd.h>
#include <linux/kallsyms.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#ifdef CONFIG_IBIP
#include <lwk/params.h>
#endif
#include "base.h"

int (*platform_notify)(struct device *dev) = NULL;
int (*platform_notify_remove)(struct device *dev) = NULL;
static struct kobject *dev_kobj;
struct kobject *sysfs_dev_char_kobj;
struct kobject *sysfs_dev_block_kobj;

#ifdef CONFIG_BLOCK
static inline int device_is_not_partition(struct device *dev)
{
	return !(dev->type == &part_type);
}
#else
static inline int device_is_not_partition(struct device *dev)
{
	return 1;
}
#endif

/**
 * dev_driver_string - Return a device's driver name, if at all possible
 * @dev: struct device to get the name of
 *
 * Will return the device's driver's name if it is bound to a device.  If
 * the device is not bound to a device, it will return the name of the bus
 * it is attached to.  If it is not attached to a bus either, an empty
 * string will be returned.
 */
const char *dev_driver_string(const struct device *dev)
{
	return dev->driver ? dev->driver->name :
			(dev->bus ? dev->bus->name :
			(dev->class ? dev->class->name : ""));
}
EXPORT_SYMBOL(dev_driver_string);

#define to_dev(obj) container_of(obj, struct device, kobj)
#define to_dev_attr(_attr) container_of(_attr, struct device_attribute, attr)

static ssize_t dev_attr_show(struct kobject *kobj, struct attribute *attr,
			     char *buf)
{
	struct device_attribute *dev_attr = to_dev_attr(attr);
	struct device *dev = to_dev(kobj);
	ssize_t ret = -EIO;

	if (dev_attr->show)
		ret = dev_attr->show(dev, dev_attr, buf);
	if (ret >= (ssize_t)PAGE_SIZE) {
		print_symbol("dev_attr_show: %s returned bad count\n",
				(unsigned long)dev_attr->show);
	}
	return ret;
}

static ssize_t dev_attr_store(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	struct device_attribute *dev_attr = to_dev_attr(attr);
	struct device *dev = to_dev(kobj);
	ssize_t ret = -EIO;

	if (dev_attr->store)
		ret = dev_attr->store(dev, dev_attr, buf, count);
	return ret;
}

static struct sysfs_ops dev_sysfs_ops = {
	.show	= dev_attr_show,
	.store	= dev_attr_store,
};


/**
 *	device_release - free device structure.
 *	@kobj:	device's kobject.
 *
 *	This is called once the reference count for the object
 *	reaches 0. We forward the call to the device's release
 *	method, which should handle actually freeing the structure.
 */
static void device_release(struct kobject *kobj)
{
	struct device *dev = to_dev(kobj);

	if (dev->release)
		dev->release(dev);
	else if (dev->type && dev->type->release)
		dev->type->release(dev);
	else if (dev->class && dev->class->dev_release)
		dev->class->dev_release(dev);
	else
		WARN(1, KERN_ERR "Device '%s' does not have a release() "
			"function, it is broken and must be fixed.\n",
			dev->bus_id);
}

static struct kobj_type device_ktype = {
	.release	= device_release,
	.sysfs_ops	= &dev_sysfs_ops,
};



static int device_add_attributes(struct device *dev,
				 struct device_attribute *attrs)
{
	int error = 0;
	int i;

	if (attrs) {
		for (i = 0; attr_name(attrs[i]); i++) {
			error = device_create_file(dev, &attrs[i]);
			if (error)
				break;
		}
		if (error)
			while (--i >= 0)
				device_remove_file(dev, &attrs[i]);
	}
	return error;
}

static void device_remove_attributes(struct device *dev,
				     struct device_attribute *attrs)
{
	int i;

	if (attrs)
		for (i = 0; attr_name(attrs[i]); i++)
			device_remove_file(dev, &attrs[i]);
}

static int device_add_groups(struct device *dev,
			     struct attribute_group **groups)
{
	int error = 0;
	int i;

	if (groups) {
		for (i = 0; groups[i]; i++) {
			error = sysfs_create_group(&dev->kobj, groups[i]);
			if (error) {
				while (--i >= 0)
					sysfs_remove_group(&dev->kobj,
							   groups[i]);
				break;
			}
		}
	}
	return error;
}

static void device_remove_groups(struct device *dev,
				 struct attribute_group **groups)
{
	int i;

	if (groups)
		for (i = 0; groups[i]; i++)
			sysfs_remove_group(&dev->kobj, groups[i]);
}

static int device_add_attrs(struct device *dev)
{
	struct class *class = dev->class;
	struct device_type *type = dev->type;
	int error;

	if (class) {
		error = device_add_attributes(dev, class->dev_attrs);
		if (error)
			return error;
	}

	if (type) {
		error = device_add_groups(dev, type->groups);
		if (error)
			goto err_remove_class_attrs;
	}

	error = device_add_groups(dev, dev->groups);
	if (error)
		goto err_remove_type_groups;

	return 0;

 err_remove_type_groups:
	if (type)
		device_remove_groups(dev, type->groups);
 err_remove_class_attrs:
	if (class)
		device_remove_attributes(dev, class->dev_attrs);

	return error;
}

static void device_remove_attrs(struct device *dev)
{
	struct class *class = dev->class;
	struct device_type *type = dev->type;

	device_remove_groups(dev, dev->groups);

	if (type)
		device_remove_groups(dev, type->groups);

	if (class)
		device_remove_attributes(dev, class->dev_attrs);
}


static ssize_t show_dev(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return print_dev_t(buf, dev->devt);
}

static struct device_attribute devt_attr =
	__ATTR(dev, S_IRUGO, show_dev, NULL);

/* kset to create /sys/devices/  */
struct kset *devices_kset;

/**
 * device_create_file - create sysfs attribute file for device.
 * @dev: device.
 * @attr: device attribute descriptor.
 */
int device_create_file(struct device *dev, struct device_attribute *attr)
{
	int error = 0;
	if (dev)
		error = sysfs_create_file(&dev->kobj, &attr->attr);
	return error;
}

/**
 * device_remove_file - remove sysfs attribute file.
 * @dev: device.
 * @attr: device attribute descriptor.
 */
void device_remove_file(struct device *dev, struct device_attribute *attr)
{
	if (dev)
		sysfs_remove_file(&dev->kobj, &attr->attr);
}

/**
 * device_create_bin_file - create sysfs binary attribute file for device.
 * @dev: device.
 * @attr: device binary attribute descriptor.
 */
int device_create_bin_file(struct device *dev, struct bin_attribute *attr)
{
	int error = -EINVAL;
	if (dev)
		error = sysfs_create_bin_file(&dev->kobj, attr);
	return error;
}
EXPORT_SYMBOL_GPL(device_create_bin_file);

/**
 * device_remove_bin_file - remove sysfs binary attribute file
 * @dev: device.
 * @attr: device binary attribute descriptor.
 */
void device_remove_bin_file(struct device *dev, struct bin_attribute *attr)
{
	if (dev)
		sysfs_remove_bin_file(&dev->kobj, attr);
}
EXPORT_SYMBOL_GPL(device_remove_bin_file);


static void klist_children_get(struct klist_node *n)
{
	struct device *dev = container_of(n, struct device, knode_parent);

	get_device(dev);
}

static void klist_children_put(struct klist_node *n)
{
	struct device *dev = container_of(n, struct device, knode_parent);

	put_device(dev);
}

/**
 * device_initialize - init device structure.
 * @dev: device.
 *
 * This prepares the device for use by other layers by initializing
 * its fields.
 * It is the first half of device_register(), if called by
 * that function, though it can also be called separately, so one
 * may use @dev's fields. In particular, get_device()/put_device()
 * may be used for reference counting of @dev after calling this
 * function.
 *
 * NOTE: Use put_device() to give up your reference instead of freeing
 * @dev directly once you have called this function.
 */
void device_initialize(struct device *dev)
{
	dev->kobj.kset = devices_kset;
	kobject_init(&dev->kobj, &device_ktype);
	klist_init(&dev->klist_children, klist_children_get,
		   klist_children_put);
	INIT_LIST_HEAD(&dev->dma_pools);
	INIT_LIST_HEAD(&dev->node);
	init_MUTEX(&dev->sem);
	set_dev_node(dev, -1);
}

#ifdef CONFIG_SYSFS_DEPRECATED
static struct kobject *get_device_parent(struct device *dev,
					 struct device *parent)
{
	/* class devices without a parent live in /sys/class/<classname>/ */
	if (dev->class && (!parent || parent->class != dev->class))
		return &dev->class->p->class_subsys.kobj;
	/* all other devices keep their parent */
	else if (parent)
		return &parent->kobj;

	return NULL;
}

static inline void cleanup_device_parent(struct device *dev) {}
static inline void cleanup_glue_dir(struct device *dev,
				    struct kobject *glue_dir) {}
#else
static struct kobject *virtual_device_parent(struct device *dev)
{
	static struct kobject *virtual_dir = NULL;

	if (!virtual_dir)
		virtual_dir = kobject_create_and_add("virtual",
						     &devices_kset->kobj);

	return virtual_dir;
}

static struct kobject *get_device_parent(struct device *dev,
					 struct device *parent)
{
	int retval;

	if (dev->class) {
		struct kobject *kobj = NULL;
		struct kobject *parent_kobj;
		struct kobject *k;

		/*
		 * If we have no parent, we live in "virtual".
		 * Class-devices with a non class-device as parent, live
		 * in a "glue" directory to prevent namespace collisions.
		 */
		if (parent == NULL)
			parent_kobj = virtual_device_parent(dev);
		else if (parent->class)
			return &parent->kobj;
		else
			parent_kobj = &parent->kobj;

		/* find our class-directory at the parent and reference it */
		spin_lock(&dev->class->p->class_dirs.list_lock);
		list_for_each_entry(k, &dev->class->p->class_dirs.list, entry)
			if (k->parent == parent_kobj) {
				kobj = kobject_get(k);
				break;
			}
		spin_unlock(&dev->class->p->class_dirs.list_lock);
		if (kobj)
			return kobj;

		/* or create a new class-directory at the parent device */
		if (dev->class && (!parent || parent->class != dev->class)) {
			printk("Using class fix from depricated_sysfs %s %p\n",
				kobject_name(&dev->class->p->class_subsys.kobj),
				&dev->class->p->class_subsys.kobj);
			return &dev->class->p->class_subsys.kobj;
		}

		k = kobject_create();
		if (!k)
			return NULL;
		k->kset = &dev->class->p->class_dirs;
		retval = kobject_add(k, parent_kobj, "%s", dev->class->name);
		if (retval < 0) {
			kobject_put(k);
			return NULL;
		}
		/* do not emit an uevent for this simple "glue" directory */
		return k;
	}

	if (parent)
		return &parent->kobj;
	return NULL;
}

static void cleanup_glue_dir(struct device *dev, struct kobject *glue_dir)
{
	/* see if we live in a "glue" directory */
	if (!glue_dir || !dev->class ||
	    glue_dir->kset != &dev->class->p->class_dirs)
		return;

	kobject_put(glue_dir);
}

static void cleanup_device_parent(struct device *dev)
{
	cleanup_glue_dir(dev, dev->kobj.parent);
}
#endif

static void setup_parent(struct device *dev, struct device *parent)
{
	struct kobject *kobj;
	kobj = get_device_parent(dev, parent);
	if (kobj)
		dev->kobj.parent = kobj;
}

static int device_add_class_symlinks(struct device *dev)
{
	int error;

	if (!dev->class)
		return 0;

	error = sysfs_create_link(&dev->kobj,
				  &dev->class->p->class_subsys.kobj,
				  "subsystem");
	if (error)
		goto out;

#ifdef CONFIG_SYSFS_DEPRECATED
	/* stacked class devices need a symlink in the class directory */
	if (dev->kobj.parent != &dev->class->p->class_subsys.kobj &&
	    device_is_not_partition(dev)) {
		error = sysfs_create_link(&dev->class->p->class_subsys.kobj,
					  &dev->kobj, dev->bus_id);
		if (error)
			goto out_subsys;
	}

	if (dev->parent && device_is_not_partition(dev)) {
		struct device *parent = dev->parent;
		char *class_name;

		/*
		 * stacked class devices have the 'device' link
		 * pointing to the bus device instead of the parent
		 */
		while (parent->class && !parent->bus && parent->parent)
			parent = parent->parent;

		error = sysfs_create_link(&dev->kobj,
					  &parent->kobj,
					  "device");
		if (error)
			goto out_busid;

		class_name = make_class_name(dev->class->name,
						&dev->kobj);
		if (class_name)
			error = sysfs_create_link(&dev->parent->kobj,
						&dev->kobj, class_name);
		kfree(class_name);
		if (error)
			goto out_device;
	}
	return 0;

out_device:
	if (dev->parent && device_is_not_partition(dev))
		sysfs_remove_link(&dev->kobj, "device");
out_busid:
	if (dev->kobj.parent != &dev->class->p->class_subsys.kobj &&
	    device_is_not_partition(dev))
		sysfs_remove_link(&dev->class->p->class_subsys.kobj,
				  dev->bus_id);
#else
	/* link in the class directory pointing to the device */
	error = sysfs_create_link(&dev->class->p->class_subsys.kobj,
				  &dev->kobj, dev->bus_id);
	if (error)
		goto out_subsys;

	if (dev->parent && device_is_not_partition(dev)) {
		error = sysfs_create_link(&dev->kobj, &dev->parent->kobj,
					  "device");
		if (error)
			goto out_busid;
	}
	return 0;

out_busid:
	sysfs_remove_link(&dev->class->p->class_subsys.kobj, dev->bus_id);
#endif

out_subsys:
	sysfs_remove_link(&dev->kobj, "subsystem");
out:
	return error;
}

static void device_remove_class_symlinks(struct device *dev)
{
	if (!dev->class)
		return;

#ifdef CONFIG_SYSFS_DEPRECATED
	if (dev->parent && device_is_not_partition(dev)) {
		char *class_name;

		class_name = make_class_name(dev->class->name, &dev->kobj);
		if (class_name) {
			sysfs_remove_link(&dev->parent->kobj, class_name);
			kfree(class_name);
		}
		sysfs_remove_link(&dev->kobj, "device");
	}

	if (dev->kobj.parent != &dev->class->p->class_subsys.kobj &&
	    device_is_not_partition(dev))
		sysfs_remove_link(&dev->class->p->class_subsys.kobj,
				  dev->bus_id);
#else
	if (dev->parent && device_is_not_partition(dev))
		sysfs_remove_link(&dev->kobj, "device");

	sysfs_remove_link(&dev->class->p->class_subsys.kobj, dev->bus_id);
#endif

	sysfs_remove_link(&dev->kobj, "subsystem");
}

/**
 * dev_set_name - set a device name
 * @dev: device
 * @fmt: format string for the device's name
 */
int dev_set_name(struct device *dev, const char *fmt, ...)
{
	va_list vargs;

	va_start(vargs, fmt);
	vsnprintf(dev->bus_id, sizeof(dev->bus_id), fmt, vargs);
	va_end(vargs);
	return 0;
}
EXPORT_SYMBOL_GPL(dev_set_name);

/**
 * device_to_dev_kobj - select a /sys/dev/ directory for the device
 * @dev: device
 *
 * By default we select char/ for new entries.  Setting class->dev_obj
 * to NULL prevents an entry from being created.  class->dev_kobj must
 * be set (or cleared) before any devices are registered to the class
 * otherwise device_create_sys_dev_entry() and
 * device_remove_sys_dev_entry() will disagree about the the presence
 * of the link.
 */
static struct kobject *device_to_dev_kobj(struct device *dev)
{
	struct kobject *kobj;

	if (dev->class)
		kobj = dev->class->dev_kobj;
	else
		kobj = sysfs_dev_char_kobj;

	return kobj;
}

extern void create_dev(char * , int , int );
static int device_create_sys_dev_entry(struct device *dev)
{
	struct kobject *kobj = device_to_dev_kobj(dev);
	int error = 0;
	char devt_str[15];
	char devfs_path[256];

	printk("mknod : sysfs %s | %s\n",kobject_name(dev->kobj.parent),
		kobject_name(&dev->kobj) );

	sprintf(devfs_path, "/dev/infiniband/%s", kobject_name(&dev->kobj) );
	if (strcmp(kobject_name(kobj),"char") == 0) {
		create_dev(devfs_path, MAJOR(dev->devt), MINOR(dev->devt));
	}

	if (kobj) {
		format_dev_t(devt_str, dev->devt);
		error = sysfs_create_link(kobj, &dev->kobj, devt_str);
	}

	return error;
}

static void device_remove_sys_dev_entry(struct device *dev)
{
	struct kobject *kobj = device_to_dev_kobj(dev);
	char devt_str[15];

	if (kobj) {
		format_dev_t(devt_str, dev->devt);
		sysfs_remove_link(kobj, devt_str);
	}
}

/**
 * device_add - add device to device hierarchy.
 * @dev: device.
 *
 * This is part 2 of device_register(), though may be called
 * separately _iff_ device_initialize() has been called separately.
 *
 * This adds @dev to the kobject hierarchy via kobject_add(), adds it
 * to the global and sibling lists for the device, then
 * adds it to the other relevant subsystems of the driver model.
 *
 * NOTE: _Never_ directly free @dev after calling this function, even
 * if it returned an error! Always use put_device() to give up your
 * reference instead.
 */
#ifdef CONFIG_IB_IP
static char ibdev_ip_str[1024] = { 0 };
param_string(ibdev_ip, ibdev_ip_str, sizeof(ibdev_ip_str));
#endif

int device_add(struct device *dev)
{
	struct device *parent = NULL;
	struct class_interface *class_intf;
	int error = -EINVAL;

	dev = get_device(dev);
	if (!dev)
		goto done;

	/* Temporarily support init_name if it is set.
	 * It will override bus_id for now */
	if (dev->init_name)
		dev_set_name(dev, "%s", dev->init_name);

	if (!strlen(dev->bus_id))
		goto done;

	pr_debug("device: '%s': %s\n", dev->bus_id, __func__);

#ifdef CONFIG_IB_IP
	/* Adding the ability to set node_desc to IP */
	struct ib_device_modify {
		u64     sys_image_guid;
		char    node_desc[64];
	};
	struct ibdev {
		char name[32];
		struct ib_device_modify desc;
	};
	struct ibdev ibdevs[8];

	struct ib_device;
	extern int ib_modify_device(struct ib_device *device,
		     int device_modify_mask,
		     struct ib_device_modify *device_modify);
	int i = 0, n_ibdev = 0, ip = 0, j = 0;

	while (ibdev_ip_str[i] != '\0') {
		if (ibdev_ip_str[i] == '\'') {
			i++;
			continue;
		}
		if (ibdev_ip_str[i] == '=') {
			ip = 1;
			ibdevs[n_ibdev].name[j] = '\0';
			j = 0;
			i++;
		}
		if (ibdev_ip_str[i] == ',') {
			ip = 0;
			ibdevs[n_ibdev].desc.node_desc[j] = '\0';
			j = 0;
			n_ibdev++;
			i++;
		}
		if (!ip)
			ibdevs[n_ibdev].name[j++] = ibdev_ip_str[i++];
		if (ip)
			ibdevs[n_ibdev].desc.node_desc[j++] = ibdev_ip_str[i++];
	}
	ibdevs[n_ibdev].desc.node_desc[j] = '\0';

	if (i!=0 && n_ibdev==0)
		n_ibdev++;

#if 0
	for (i=0;i<n_ibdev; i++)
		printk("device table %d : %s : %s\n", i, ibdevs[i].name,
			ibdevs[i].desc.node_desc);
#endif

	if (dev->driver_data)
		for (i=0;i<n_ibdev; i++)
			if (!strcmp(dev->bus_id, ibdevs[i].name)) {
				printk("device: '%s': %s\n", dev->bus_id,
					ibdevs[i].desc.node_desc);
				ib_modify_device(dev->driver_data,
						(1 << 1), &(ibdevs[i].desc));
				break;
			}
	/**  End  **/
#endif
	parent = get_device(dev->parent);
	setup_parent(dev, parent);

	/* use parent numa_node */
	if (parent)
		set_dev_node(dev, dev_to_node(parent));

	/* first, register with generic layer. */
	error = kobject_add(&dev->kobj, dev->kobj.parent, "%s", dev->bus_id);
	if (error)
		goto Error;

	/* notify platform of device entry */
	if (platform_notify)
		platform_notify(dev);

	/* notify clients of device entry (new way) */
	if (dev->bus)
		blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
					     BUS_NOTIFY_ADD_DEVICE, dev);


	if (MAJOR(dev->devt)) {
		error = device_create_file(dev, &devt_attr);
		if (error)
			goto attrError;

		error = device_create_sys_dev_entry(dev);
		if (error)
			goto devtattrError;
	}

	error = device_add_class_symlinks(dev);
	if (error)
		goto SymlinkError;
	error = device_add_attrs(dev);
	if (error)
		goto AttrsError;
	error = bus_add_device(dev);
	if (error)
		goto BusError;

	bus_attach_device(dev);
	if (parent)
		klist_add_tail(&dev->knode_parent, &parent->klist_children);

	if (dev->class) {
		mutex_lock(&dev->class->p->class_mutex);
		/* tie the class to the device */
		list_add_tail(&dev->node, &dev->class->p->class_devices);

		/* notify any interfaces that the device is here */
		list_for_each_entry(class_intf,
				    &dev->class->p->class_interfaces, node)
			if (class_intf->add_dev)
				class_intf->add_dev(dev, class_intf);
		mutex_unlock(&dev->class->p->class_mutex);
	}
done:
	put_device(dev);
	return error;
 BusError:
	if (dev->bus)
		blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
					     BUS_NOTIFY_DEL_DEVICE, dev);
	device_remove_attrs(dev);
 AttrsError:
	device_remove_class_symlinks(dev);
 SymlinkError:
	if (MAJOR(dev->devt))
		device_remove_sys_dev_entry(dev);
 devtattrError:
	if (MAJOR(dev->devt))
		device_remove_file(dev, &devt_attr);
 attrError:
	kobject_del(&dev->kobj);
 Error:
	cleanup_device_parent(dev);
	if (parent)
		put_device(parent);
	goto done;
}

/**
 * device_register - register a device with the system.
 * @dev: pointer to the device structure
 *
 * This happens in two clean steps - initialize the device
 * and add it to the system. The two steps can be called
 * separately, but this is the easiest and most common.
 * I.e. you should only call the two helpers separately if
 * have a clearly defined need to use and refcount the device
 * before it is added to the hierarchy.
 *
 * NOTE: _Never_ directly free @dev after calling this function, even
 * if it returned an error! Always use put_device() to give up the
 * reference initialized in this function instead.
 */
int device_register(struct device *dev)
{
	device_initialize(dev);
	return device_add(dev);
}

/**
 * get_device - increment reference count for device.
 * @dev: device.
 *
 * This simply forwards the call to kobject_get(), though
 * we do take care to provide for the case that we get a NULL
 * pointer passed in.
 */
struct device *get_device(struct device *dev)
{
	return dev ? to_dev(kobject_get(&dev->kobj)) : NULL;
}

/**
 * put_device - decrement reference count.
 * @dev: device in question.
 */
void put_device(struct device *dev)
{
	/* might_sleep(); */
	if (dev)
		kobject_put(&dev->kobj);
}

/**
 * device_del - delete device from system.
 * @dev: device.
 *
 * This is the first part of the device unregistration
 * sequence. This removes the device from the lists we control
 * from here, has it removed from the other driver model
 * subsystems it was added to in device_add(), and removes it
 * from the kobject hierarchy.
 *
 * NOTE: this should be called manually _iff_ device_add() was
 * also called manually.
 */
void device_del(struct device *dev)
{
	struct device *parent = dev->parent;
	struct class_interface *class_intf;

	if (parent)
		klist_del(&dev->knode_parent);
	if (MAJOR(dev->devt)) {
		device_remove_sys_dev_entry(dev);
		device_remove_file(dev, &devt_attr);
	}
	if (dev->class) {
		device_remove_class_symlinks(dev);

		mutex_lock(&dev->class->p->class_mutex);
		/* notify any interfaces that the device is now gone */
		list_for_each_entry(class_intf,
				    &dev->class->p->class_interfaces, node)
			if (class_intf->remove_dev)
				class_intf->remove_dev(dev, class_intf);
		/* remove the device from the class list */
		list_del_init(&dev->node);
		mutex_unlock(&dev->class->p->class_mutex);
	}
	device_remove_attrs(dev);
	bus_remove_device(dev);


	/* Notify the platform of the removal, in case they
	 * need to do anything...
	 */
	if (platform_notify_remove)
		platform_notify_remove(dev);
	if (dev->bus)
		blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
					     BUS_NOTIFY_DEL_DEVICE, dev);
	cleanup_device_parent(dev);
	kobject_del(&dev->kobj);
	put_device(parent);
}

/**
 * device_unregister - unregister device from system.
 * @dev: device going away.
 *
 * We do this in two parts, like we do device_register(). First,
 * we remove it from all the subsystems with device_del(), then
 * we decrement the reference count via put_device(). If that
 * is the final reference count, the device will be cleaned up
 * via device_release() above. Otherwise, the structure will
 * stick around until the final reference to the device is dropped.
 */
void device_unregister(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev->bus_id, __func__);
	device_del(dev);
	put_device(dev);
}

static struct device *next_device(struct klist_iter *i)
{
	struct klist_node *n = klist_next(i);
	return n ? container_of(n, struct device, knode_parent) : NULL;
}

/**
 * device_for_each_child - device child iterator.
 * @parent: parent struct device.
 * @data: data for the callback.
 * @fn: function to be called for each device.
 *
 * Iterate over @parent's child devices, and call @fn for each,
 * passing it @data.
 *
 * We check the return of @fn each time. If it returns anything
 * other than 0, we break out and return that value.
 */
int device_for_each_child(struct device *parent, void *data,
			  int (*fn)(struct device *dev, void *data))
{
	struct klist_iter i;
	struct device *child;
	int error = 0;

	klist_iter_init(&parent->klist_children, &i);
	while ((child = next_device(&i)) && !error)
		error = fn(child, data);
	klist_iter_exit(&i);
	return error;
}


int __init devices_init(void)
{
    devices_kset = kset_create_and_add("devices", NULL, NULL);
	if (!devices_kset)
		return -ENOMEM;
	dev_kobj = kobject_create_and_add("dev", NULL);
	if (!dev_kobj)
		goto dev_kobj_err;
	sysfs_dev_block_kobj = kobject_create_and_add("block", dev_kobj);
	if (!sysfs_dev_block_kobj)
		goto block_kobj_err;
	sysfs_dev_char_kobj = kobject_create_and_add("char", dev_kobj);
	if (!sysfs_dev_char_kobj)
		goto char_kobj_err;

	return 0;

 char_kobj_err:
	kobject_put(sysfs_dev_block_kobj);
 block_kobj_err:
	kobject_put(dev_kobj);
 dev_kobj_err:
	kset_unregister(devices_kset);
	return -ENOMEM;
}

EXPORT_SYMBOL_GPL(device_for_each_child);
EXPORT_SYMBOL_GPL(device_find_child);

EXPORT_SYMBOL_GPL(device_initialize);
EXPORT_SYMBOL_GPL(device_add);
EXPORT_SYMBOL_GPL(device_register);

EXPORT_SYMBOL_GPL(device_del);
EXPORT_SYMBOL_GPL(device_unregister);
EXPORT_SYMBOL_GPL(get_device);
EXPORT_SYMBOL_GPL(put_device);

EXPORT_SYMBOL_GPL(device_create_file);
EXPORT_SYMBOL_GPL(device_remove_file);


static void device_create_release(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev->bus_id, __func__);
	kfree(dev);
}

/**
 * device_create_vargs - creates a device and registers it with sysfs
 * @class: pointer to the struct class that this device should be registered to
 * @parent: pointer to the parent struct device of this new device, if any
 * @devt: the dev_t for the char device to be added
 * @drvdata: the data to be added to the device for callbacks
 * @fmt: string for the device's name
 * @args: va_list for the device's name
 *
 * This function can be used by char device classes.  A struct device
 * will be created in sysfs, registered to the specified class.
 *
 * A "dev" file will be created, showing the dev_t for the device, if
 * the dev_t is not 0,0.
 * If a pointer to a parent struct device is passed in, the newly created
 * struct device will be a child of that device in sysfs.
 * The pointer to the struct device will be returned from the call.
 * Any further sysfs files that might be required can be created using this
 * pointer.
 *
 * Note: the struct class passed to this function must have previously
 * been created with a call to class_create().
 */
struct device *device_create_vargs(struct class *class, struct device *parent,
				   dev_t devt, void *drvdata, const char *fmt,
				   va_list args)
{
	struct device *dev = NULL;
	int retval = -ENODEV;

	if (class == NULL || IS_ERR(class))
		goto error;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		retval = -ENOMEM;
		goto error;
	}

	dev->devt = devt;
	dev->class = class;
	dev->parent = parent;
	dev->release = device_create_release;
	dev_set_drvdata(dev, drvdata);

	vsnprintf(dev->bus_id, BUS_ID_SIZE, fmt, args);
	retval = device_register(dev);
	if (retval)
		goto error;

	return dev;

error:
	put_device(dev);
	return ERR_PTR(retval);
}
EXPORT_SYMBOL_GPL(device_create_vargs);

/**
 * device_create - creates a device and registers it with sysfs
 * @class: pointer to the struct class that this device should be registered to
 * @parent: pointer to the parent struct device of this new device, if any
 * @devt: the dev_t for the char device to be added
 * @drvdata: the data to be added to the device for callbacks
 * @fmt: string for the device's name
 *
 * This function can be used by char device classes.  A struct device
 * will be created in sysfs, registered to the specified class.
 *
 * A "dev" file will be created, showing the dev_t for the device, if
 * the dev_t is not 0,0.
 * If a pointer to a parent struct device is passed in, the newly created
 * struct device will be a child of that device in sysfs.
 * The pointer to the struct device will be returned from the call.
 * Any further sysfs files that might be required can be created using this
 * pointer.
 *
 * Note: the struct class passed to this function must have previously
 * been created with a call to class_create().
 */
struct device *device_create(struct class *class, struct device *parent,
			     dev_t devt, void *drvdata, const char *fmt, ...)
{
	va_list vargs;
	struct device *dev;

	va_start(vargs, fmt);
	dev = device_create_vargs(class, parent, devt, drvdata, fmt, vargs);
	va_end(vargs);
	return dev;
}
EXPORT_SYMBOL_GPL(device_create);

static int __match_devt(struct device *dev, void *data)
{
	dev_t *devt = data;

	return dev->devt == *devt;
}

/**
 * device_destroy - removes a device that was created with device_create()
 * @class: pointer to the struct class that this device was registered with
 * @devt: the dev_t of the device that was previously registered
 *
 * This call unregisters and cleans up a device that was created with a
 * call to device_create().
 */
void device_destroy(struct class *class, dev_t devt)
{
	struct device *dev;

	dev = class_find_device(class, NULL, &devt, __match_devt);
	if (dev) {
		put_device(dev);
		device_unregister(dev);
	}
}
EXPORT_SYMBOL_GPL(device_destroy);





/**
 * device_shutdown - call ->shutdown() on each device to shutdown.
 */
void device_shutdown(void)
{
	struct device *dev, *devn;

	list_for_each_entry_safe_reverse(dev, devn, &devices_kset->list,
				kobj.entry) {
		if (dev->bus && dev->bus->shutdown) {
			dev_dbg(dev, "shutdown\n");
			dev->bus->shutdown(dev);
		} else if (dev->driver && dev->driver->shutdown) {
			dev_dbg(dev, "shutdown\n");
			dev->driver->shutdown(dev);
		}
	}
	kobject_put(sysfs_dev_char_kobj);
	kobject_put(sysfs_dev_block_kobj);
	kobject_put(dev_kobj);
}

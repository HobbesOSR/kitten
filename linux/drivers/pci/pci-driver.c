/*
 * drivers/pci/pci-driver.c
 *
 * (C) Copyright 2002-2004, 2007 Greg Kroah-Hartman <greg@kroah.com>
 * (C) Copyright 2007 Novell Inc.
 *
 * Released under the GPL v2 only.
 *
 */

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mempolicy.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include "pci.h"

/*
 * Dynamic device IDs are disabled for !CONFIG_HOTPLUG
 */

struct pci_dynid {
	struct list_head node;
	struct pci_device_id id;
};

#ifdef CONFIG_HOTPLUG

/**
 * store_new_id - add a new PCI device ID to this driver and re-probe devices
 * @driver: target device driver
 * @buf: buffer for scanning device ID data
 * @count: input size
 *
 * Adds a new dynamic pci device ID to this driver,
 * and causes the driver to probe for all devices again.
 */
static ssize_t
store_new_id(struct device_driver *driver, const char *buf, size_t count)
{
	struct pci_dynid *dynid;
	struct pci_driver *pdrv = to_pci_driver(driver);
	__u32 vendor, device, subvendor=PCI_ANY_ID,
		subdevice=PCI_ANY_ID, class=0, class_mask=0;
	unsigned long driver_data=0;
	int fields=0;
	int retval = 0;

	fields = sscanf(buf, "%x %x %x %x %x %x %lux",
			&vendor, &device, &subvendor, &subdevice,
			&class, &class_mask, &driver_data);
	if (fields < 2)
		return -EINVAL;

	dynid = kzalloc(sizeof(*dynid), GFP_KERNEL);
	if (!dynid)
		return -ENOMEM;

	dynid->id.vendor = vendor;
	dynid->id.device = device;
	dynid->id.subvendor = subvendor;
	dynid->id.subdevice = subdevice;
	dynid->id.class = class;
	dynid->id.class_mask = class_mask;
	dynid->id.driver_data = pdrv->dynids.use_driver_data ?
		driver_data : 0UL;

	spin_lock(&pdrv->dynids.lock);
	list_add_tail(&dynid->node, &pdrv->dynids.list);
	spin_unlock(&pdrv->dynids.lock);

	if (get_driver(&pdrv->driver)) {
		retval = driver_attach(&pdrv->driver);
		put_driver(&pdrv->driver);
	}

	if (retval)
		return retval;
	return count;
}
static DRIVER_ATTR(new_id, S_IWUSR, NULL, store_new_id);

static void
pci_free_dynids(struct pci_driver *drv)
{
	struct pci_dynid *dynid, *n;

	spin_lock(&drv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &drv->dynids.list, node) {
		list_del(&dynid->node);
		kfree(dynid);
	}
	spin_unlock(&drv->dynids.lock);
}

static int
pci_create_newid_file(struct pci_driver *drv)
{
	int error = 0;
	if (drv->probe != NULL)
		error = driver_create_file(&drv->driver, &driver_attr_new_id);
	return error;
}

static void pci_remove_newid_file(struct pci_driver *drv)
{
	driver_remove_file(&drv->driver, &driver_attr_new_id);
}
#else /* !CONFIG_HOTPLUG */
static inline void pci_free_dynids(struct pci_driver *drv) {}
static inline int pci_create_newid_file(struct pci_driver *drv)
{
	return 0;
}
static inline void pci_remove_newid_file(struct pci_driver *drv) {}
#endif

/**
 * pci_match_id - See if a pci device matches a given pci_id table
 * @ids: array of PCI device id structures to search in
 * @dev: the PCI device structure to match against.
 *
 * Used by a driver to check whether a PCI device present in the
 * system is in its list of supported devices.  Returns the matching
 * pci_device_id structure or %NULL if there is no match.
 *
 * Deprecated, don't use this as it will not catch any dynamic ids
 * that a driver might want to check for.
 */
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
					 struct pci_dev *dev)
{
	if (ids) {
		while (ids->vendor || ids->subvendor || ids->class_mask) {
			if (pci_match_one_device(ids, dev))
				return ids;
			ids++;
		}
	}
	return NULL;
}

/**
 * pci_match_device - Tell if a PCI device structure has a matching PCI device id structure
 * @drv: the PCI driver to match against
 * @dev: the PCI device structure to match against
 *
 * Used by a driver to check whether a PCI device present in the
 * system is in its list of supported devices.  Returns the matching
 * pci_device_id structure or %NULL if there is no match.
 */
static const struct pci_device_id *pci_match_device(struct pci_driver *drv,
						    struct pci_dev *dev)
{
	struct pci_dynid *dynid;

	/* Look at the dynamic ids first, before the static ones */
	spin_lock(&drv->dynids.lock);
	list_for_each_entry(dynid, &drv->dynids.list, node) {
		if (pci_match_one_device(&dynid->id, dev)) {
			spin_unlock(&drv->dynids.lock);
			return &dynid->id;
		}
	}
	spin_unlock(&drv->dynids.lock);

	return pci_match_id(drv->id_table, dev);
}

static int pci_call_probe(struct pci_driver *drv, struct pci_dev *dev,
			  const struct pci_device_id *id)
{
	int error;
#ifdef CONFIG_NUMA
	/* Execute driver initialization on node where the
	   device's bus is attached to.  This way the driver likely
	   allocates its local memory on the right node without
	   any need to change it. */
	struct mempolicy *oldpol;
	cpumask_t oldmask = current->cpus_allowed;
	int node = dev_to_node(&dev->dev);

	if (node >= 0) {
		node_to_cpumask_ptr(nodecpumask, node);
		set_cpus_allowed_ptr(current, nodecpumask);
	}
	/* And set default memory allocation policy */
	oldpol = current->mempolicy;
	current->mempolicy = NULL;	/* fall back to system default policy */
#endif
	error = drv->probe(dev, id);
#ifdef CONFIG_NUMA
	set_cpus_allowed_ptr(current, &oldmask);
	current->mempolicy = oldpol;
#endif
	return error;
}

/**
 * __pci_device_probe()
 * @drv: driver to call to check if it wants the PCI device
 * @pci_dev: PCI device being probed
 * 
 * returns 0 on success, else error.
 * side-effect: pci_dev->driver is set to drv when drv claims pci_dev.
 */
static int
__pci_device_probe(struct pci_driver *drv, struct pci_dev *pci_dev)
{
	const struct pci_device_id *id;
	int error = 0;

	if (!pci_dev->driver && drv->probe) {
		error = -ENODEV;

		id = pci_match_device(drv, pci_dev);
		if (id)
			error = pci_call_probe(drv, pci_dev, id);
		if (error >= 0) {
			pci_dev->driver = drv;
			error = 0;
		}
	}
	return error;
}

static int pci_device_probe(struct device * dev)
{
	int error = 0;
	struct pci_driver *drv;
	struct pci_dev *pci_dev;

	drv = to_pci_driver(dev->driver);
	pci_dev = to_pci_dev(dev);
	pci_dev_get(pci_dev);
	error = __pci_device_probe(drv, pci_dev);
	if (error)
		pci_dev_put(pci_dev);

	return error;
}

static int pci_device_remove(struct device * dev)
{
	struct pci_dev * pci_dev = to_pci_dev(dev);
	struct pci_driver * drv = pci_dev->driver;

	if (drv) {
		if (drv->remove)
			drv->remove(pci_dev);
		pci_dev->driver = NULL;
	}

	/*
	 * If the device is still on, set the power state as "unknown",
	 * since it might change by the next time we load the driver.
	 */
	if (pci_dev->current_state == PCI_D0)
		pci_dev->current_state = PCI_UNKNOWN;

	/*
	 * We would love to complain here if pci_dev->is_enabled is set, that
	 * the driver should have called pci_disable_device(), but the
	 * unfortunate fact is there are too many odd BIOS and bridge setups
	 * that don't like drivers doing that all of the time.  
	 * Oh well, we can dream of sane hardware when we sleep, no matter how
	 * horrible the crap we have to deal with is when we are awake...
	 */

	pci_dev_put(pci_dev);
	return 0;
}

static void pci_device_shutdown(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;

	if (drv && drv->shutdown)
		drv->shutdown(pci_dev);
	pci_msi_shutdown(pci_dev);
	pci_msix_shutdown(pci_dev);
}

#ifdef CONFIG_PM_SLEEP

static bool pci_has_legacy_pm_support(struct pci_dev *pci_dev)
{
	struct pci_driver *drv = pci_dev->driver;

	return drv && (drv->suspend || drv->suspend_late || drv->resume
		|| drv->resume_early);
}

/*
 * Default "suspend" method for devices that have no driver provided suspend,
 * or not even a driver at all.
 */
static void pci_default_pm_suspend(struct pci_dev *pci_dev)
{
	pci_save_state(pci_dev);
	/*
	 * mark its power state as "unknown", since we don't know if
	 * e.g. the BIOS will change its device state when we suspend.
	 */
	if (pci_dev->current_state == PCI_D0)
		pci_dev->current_state = PCI_UNKNOWN;
}

/*
 * Default "resume" method for devices that have no driver provided resume,
 * or not even a driver at all (first part).
 */
static void pci_default_pm_resume_early(struct pci_dev *pci_dev)
{
	/* restore the PCI config space */
	pci_restore_state(pci_dev);
}

/*
 * Default "resume" method for devices that have no driver provided resume,
 * or not even a driver at all (second part).
 */
static int pci_default_pm_resume_late(struct pci_dev *pci_dev)
{
	int retval;

	/* if the device was enabled before suspend, reenable */
	retval = pci_reenable_device(pci_dev);
	/*
	 * if the device was busmaster before the suspend, make it busmaster
	 * again
	 */
	if (pci_dev->is_busmaster)
		pci_set_master(pci_dev);

	return retval;
}

static int pci_legacy_suspend(struct device *dev, pm_message_t state)
{
	struct pci_dev * pci_dev = to_pci_dev(dev);
	struct pci_driver * drv = pci_dev->driver;
	int i = 0;

	if (drv && drv->suspend) {
		i = drv->suspend(pci_dev, state);
		suspend_report_result(drv->suspend, i);
	} else {
		pci_default_pm_suspend(pci_dev);
	}
	return i;
}

static int pci_legacy_suspend_late(struct device *dev, pm_message_t state)
{
	struct pci_dev * pci_dev = to_pci_dev(dev);
	struct pci_driver * drv = pci_dev->driver;
	int i = 0;

	if (drv && drv->suspend_late) {
		i = drv->suspend_late(pci_dev, state);
		suspend_report_result(drv->suspend_late, i);
	}
	return i;
}

static int pci_legacy_resume(struct device *dev)
{
	int error;
	struct pci_dev * pci_dev = to_pci_dev(dev);
	struct pci_driver * drv = pci_dev->driver;

	if (drv && drv->resume) {
		error = drv->resume(pci_dev);
	} else {
		pci_default_pm_resume_early(pci_dev);
		error = pci_default_pm_resume_late(pci_dev);
	}
	return error;
}

static int pci_legacy_resume_early(struct device *dev)
{
	int error = 0;
	struct pci_dev * pci_dev = to_pci_dev(dev);
	struct pci_driver * drv = pci_dev->driver;

	if (drv && drv->resume_early)
		error = drv->resume_early(pci_dev);
	return error;
}

static int pci_pm_prepare(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int error = 0;

	if (drv && drv->pm && drv->pm->prepare)
		error = drv->pm->prepare(dev);

	return error;
}

static void pci_pm_complete(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	if (drv && drv->pm && drv->pm->complete)
		drv->pm->complete(dev);
}

#ifdef CONFIG_SUSPEND

static int pci_pm_suspend(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct device_driver *drv = dev->driver;
	int error = 0;

	if (drv && drv->pm) {
		if (drv->pm->suspend) {
			error = drv->pm->suspend(dev);
			suspend_report_result(drv->pm->suspend, error);
		}
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_suspend(dev, PMSG_SUSPEND);
	}
	pci_fixup_device(pci_fixup_suspend, pci_dev);

	return error;
}

static int pci_pm_suspend_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;
	int error = 0;

	if (drv && drv->pm) {
		if (drv->pm->suspend_noirq) {
			error = drv->pm->suspend_noirq(dev);
			suspend_report_result(drv->pm->suspend_noirq, error);
		}
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_suspend_late(dev, PMSG_SUSPEND);
	} else {
		pci_default_pm_suspend(pci_dev);
	}

	return error;
}

static int pci_pm_resume(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct device_driver *drv = dev->driver;
	int error = 0;

	pci_fixup_device(pci_fixup_resume, pci_dev);

	if (drv && drv->pm) {
		if (drv->pm->resume)
			error = drv->pm->resume(dev);
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_resume(dev);
	} else {
		error = pci_default_pm_resume_late(pci_dev);
	}

	return error;
}

static int pci_pm_resume_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;
	int error = 0;

	pci_fixup_device(pci_fixup_resume_early, pci_dev);

	if (drv && drv->pm) {
		if (drv->pm->resume_noirq)
			error = drv->pm->resume_noirq(dev);
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_resume_early(dev);
	} else {
		pci_default_pm_resume_early(pci_dev);
	}

	return error;
}

#else /* !CONFIG_SUSPEND */

#define pci_pm_suspend		NULL
#define pci_pm_suspend_noirq	NULL
#define pci_pm_resume		NULL
#define pci_pm_resume_noirq	NULL

#endif /* !CONFIG_SUSPEND */

#ifdef CONFIG_HIBERNATION

static int pci_pm_freeze(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct device_driver *drv = dev->driver;
	int error = 0;

	if (drv && drv->pm) {
		if (drv->pm->freeze) {
			error = drv->pm->freeze(dev);
			suspend_report_result(drv->pm->freeze, error);
		}
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_suspend(dev, PMSG_FREEZE);
		pci_fixup_device(pci_fixup_suspend, pci_dev);
	}

	return error;
}

static int pci_pm_freeze_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;
	int error = 0;

	if (drv && drv->pm) {
		if (drv->pm->freeze_noirq) {
			error = drv->pm->freeze_noirq(dev);
			suspend_report_result(drv->pm->freeze_noirq, error);
		}
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_suspend_late(dev, PMSG_FREEZE);
	} else {
		pci_default_pm_suspend(pci_dev);
	}

	return error;
}

static int pci_pm_thaw(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct device_driver *drv = dev->driver;
	int error = 0;

	if (drv && drv->pm) {
		if (drv->pm->thaw)
			error =  drv->pm->thaw(dev);
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		pci_fixup_device(pci_fixup_resume, pci_dev);
		error = pci_legacy_resume(dev);
	}

	return error;
}

static int pci_pm_thaw_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;
	int error = 0;

	if (drv && drv->pm) {
		if (drv->pm->thaw_noirq)
			error = drv->pm->thaw_noirq(dev);
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		pci_fixup_device(pci_fixup_resume_early, pci_dev);
		error = pci_legacy_resume_early(dev);
	}

	return error;
}

static int pci_pm_poweroff(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct device_driver *drv = dev->driver;
	int error = 0;

	pci_fixup_device(pci_fixup_suspend, pci_dev);

	if (drv && drv->pm) {
		if (drv->pm->poweroff) {
			error = drv->pm->poweroff(dev);
			suspend_report_result(drv->pm->poweroff, error);
		}
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_suspend(dev, PMSG_HIBERNATE);
	}

	return error;
}

static int pci_pm_poweroff_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;
	int error = 0;

	if (drv && drv->pm) {
		if (drv->pm->poweroff_noirq) {
			error = drv->pm->poweroff_noirq(dev);
			suspend_report_result(drv->pm->poweroff_noirq, error);
		}
	} else if (pci_has_legacy_pm_support(to_pci_dev(dev))) {
		error = pci_legacy_suspend_late(dev, PMSG_HIBERNATE);
	}

	return error;
}

static int pci_pm_restore(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct device_driver *drv = dev->driver;
	int error = 0;

	if (drv && drv->pm) {
		if (drv->pm->restore)
			error = drv->pm->restore(dev);
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_resume(dev);
	} else {
		error = pci_default_pm_resume_late(pci_dev);
	}
	pci_fixup_device(pci_fixup_resume, pci_dev);

	return error;
}

static int pci_pm_restore_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;
	int error = 0;

	pci_fixup_device(pci_fixup_resume, pci_dev);

	if (drv && drv->pm) {
		if (drv->pm->restore_noirq)
			error = drv->pm->restore_noirq(dev);
	} else if (pci_has_legacy_pm_support(pci_dev)) {
		error = pci_legacy_resume_early(dev);
	} else {
		pci_default_pm_resume_early(pci_dev);
	}
	pci_fixup_device(pci_fixup_resume_early, pci_dev);

	return error;
}

#else /* !CONFIG_HIBERNATION */

#define pci_pm_freeze		NULL
#define pci_pm_freeze_noirq	NULL
#define pci_pm_thaw		NULL
#define pci_pm_thaw_noirq	NULL
#define pci_pm_poweroff		NULL
#define pci_pm_poweroff_noirq	NULL
#define pci_pm_restore		NULL
#define pci_pm_restore_noirq	NULL

#endif /* !CONFIG_HIBERNATION */

struct pm_ext_ops pci_pm_ops = {
	.base = {
		.prepare = pci_pm_prepare,
		.complete = pci_pm_complete,
		.suspend = pci_pm_suspend,
		.resume = pci_pm_resume,
		.freeze = pci_pm_freeze,
		.thaw = pci_pm_thaw,
		.poweroff = pci_pm_poweroff,
		.restore = pci_pm_restore,
	},
	.suspend_noirq = pci_pm_suspend_noirq,
	.resume_noirq = pci_pm_resume_noirq,
	.freeze_noirq = pci_pm_freeze_noirq,
	.thaw_noirq = pci_pm_thaw_noirq,
	.poweroff_noirq = pci_pm_poweroff_noirq,
	.restore_noirq = pci_pm_restore_noirq,
};

#define PCI_PM_OPS_PTR	&pci_pm_ops

#else /* !CONFIG_PM_SLEEP */

#define PCI_PM_OPS_PTR	NULL

#endif /* !CONFIG_PM_SLEEP */

/**
 * __pci_register_driver - register a new pci driver
 * @drv: the driver structure to register
 * @owner: owner module of drv
 * @mod_name: module name string
 * 
 * Adds the driver structure to the list of registered drivers.
 * Returns a negative value on error, otherwise 0. 
 * If no error occurred, the driver remains registered even if 
 * no device was claimed during registration.
 */
int __pci_register_driver(struct pci_driver *drv, struct module *owner,
			  const char *mod_name)
{
	int error;

	/* initialize common driver fields */
	drv->driver.name = drv->name;
	drv->driver.bus = &pci_bus_type;
	drv->driver.owner = owner;
	drv->driver.mod_name = mod_name;

	if (drv->pm)
		drv->driver.pm = &drv->pm->base;

	spin_lock_init(&drv->dynids.lock);
	INIT_LIST_HEAD(&drv->dynids.list);

	/* register with core */
	error = driver_register(&drv->driver);
	if (error)
		return error;

	error = pci_create_newid_file(drv);
	if (error)
		driver_unregister(&drv->driver);

	return error;
}

/**
 * pci_unregister_driver - unregister a pci driver
 * @drv: the driver structure to unregister
 * 
 * Deletes the driver structure from the list of registered PCI drivers,
 * gives it a chance to clean up by calling its remove() function for
 * each device it was responsible for, and marks those devices as
 * driverless.
 */

void
pci_unregister_driver(struct pci_driver *drv)
{
	pci_remove_newid_file(drv);
	driver_unregister(&drv->driver);
	pci_free_dynids(drv);
}

static struct pci_driver pci_compat_driver = {
	.name = "compat"
};

/**
 * pci_dev_driver - get the pci_driver of a device
 * @dev: the device to query
 *
 * Returns the appropriate pci_driver structure or %NULL if there is no 
 * registered driver for the device.
 */
struct pci_driver *
pci_dev_driver(const struct pci_dev *dev)
{
	if (dev->driver)
		return dev->driver;
	else {
		int i;
		for(i=0; i<=PCI_ROM_RESOURCE; i++)
			if (dev->resource[i].flags & IORESOURCE_BUSY)
				return &pci_compat_driver;
	}
	return NULL;
}

/**
 * pci_bus_match - Tell if a PCI device structure has a matching PCI device id structure
 * @dev: the PCI device structure to match against
 * @drv: the device driver to search for matching PCI device id structures
 * 
 * Used by a driver to check whether a PCI device present in the
 * system is in its list of supported devices. Returns the matching
 * pci_device_id structure or %NULL if there is no match.
 */
static int pci_bus_match(struct device *dev, struct device_driver *drv)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *pci_drv = to_pci_driver(drv);
	const struct pci_device_id *found_id;

	found_id = pci_match_device(pci_drv, pci_dev);
	if (found_id)
		return 1;

	return 0;
}

/**
 * pci_dev_get - increments the reference count of the pci device structure
 * @dev: the device being referenced
 *
 * Each live reference to a device should be refcounted.
 *
 * Drivers for PCI devices should normally record such references in
 * their probe() methods, when they bind to a device, and release
 * them by calling pci_dev_put(), in their disconnect() methods.
 *
 * A pointer to the device with the incremented reference counter is returned.
 */
struct pci_dev *pci_dev_get(struct pci_dev *dev)
{
	if (dev)
		get_device(&dev->dev);
	return dev;
}

/**
 * pci_dev_put - release a use of the pci device structure
 * @dev: device that's been disconnected
 *
 * Must be called when a user of a device is finished with it.  When the last
 * user of the device calls this function, the memory of the device is freed.
 */
void pci_dev_put(struct pci_dev *dev)
{
	if (dev)
		put_device(&dev->dev);
}

#ifndef CONFIG_HOTPLUG
int pci_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	return -ENODEV;
}
#endif

struct bus_type pci_bus_type = {
	.name		= "pci",
	.match		= pci_bus_match,
	.uevent		= pci_uevent,
	.probe		= pci_device_probe,
	.remove		= pci_device_remove,
	.shutdown	= pci_device_shutdown,
	.dev_attrs	= pci_dev_attrs,
	.pm		= PCI_PM_OPS_PTR,
};

static int __init pci_driver_init(void)
{
	return bus_register(&pci_bus_type);
}

postcore_initcall(pci_driver_init);

EXPORT_SYMBOL(pci_match_id);
EXPORT_SYMBOL(__pci_register_driver);
EXPORT_SYMBOL(pci_unregister_driver);
EXPORT_SYMBOL(pci_dev_driver);
EXPORT_SYMBOL(pci_bus_type);
EXPORT_SYMBOL(pci_dev_get);
EXPORT_SYMBOL(pci_dev_put);

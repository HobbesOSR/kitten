/* Functions internal to the PCI core code */

extern int pci_uevent(struct device *dev, struct kobj_uevent_env *env);
extern int pci_create_sysfs_dev_files(struct pci_dev *pdev);
extern void pci_remove_sysfs_dev_files(struct pci_dev *pdev);
extern void pci_cleanup_rom(struct pci_dev *dev);

/**
 * Firmware PM callbacks
 *
 * @is_manageable - returns 'true' if given device is power manageable by the
 *                  platform firmware
 *
 * @set_state - invokes the platform firmware to set the device's power state
 *
 * @choose_state - returns PCI power state of given device preferred by the
 *                 platform; to be used during system-wide transitions from a
 *                 sleeping state to the working state and vice versa
 *
 * @can_wakeup - returns 'true' if given device is capable of waking up the
 *               system from a sleeping state
 *
 * @sleep_wake - enables/disables the system wake up capability of given device
 *
 * If given platform is generally capable of power managing PCI devices, all of
 * these callbacks are mandatory.
 */
struct pci_platform_pm_ops {
	bool (*is_manageable)(struct pci_dev *dev);
	int (*set_state)(struct pci_dev *dev, pci_power_t state);
	pci_power_t (*choose_state)(struct pci_dev *dev);
	bool (*can_wakeup)(struct pci_dev *dev);
	int (*sleep_wake)(struct pci_dev *dev, bool enable);
};

extern int pci_set_platform_pm(struct pci_platform_pm_ops *ops);
extern void pci_pm_init(struct pci_dev *dev);

extern int pci_user_read_config_byte(struct pci_dev *dev, int where, u8 *val);
extern int pci_user_read_config_word(struct pci_dev *dev, int where, u16 *val);
extern int pci_user_read_config_dword(struct pci_dev *dev, int where, u32 *val);
extern int pci_user_write_config_byte(struct pci_dev *dev, int where, u8 val);
extern int pci_user_write_config_word(struct pci_dev *dev, int where, u16 val);
extern int pci_user_write_config_dword(struct pci_dev *dev, int where, u32 val);

struct pci_vpd_ops {
	int (*read)(struct pci_dev *dev, int pos, int size, char *buf);
	int (*write)(struct pci_dev *dev, int pos, int size, const char *buf);
	void (*release)(struct pci_dev *dev);
};

struct pci_vpd {
	unsigned int len;
	struct pci_vpd_ops *ops;
	struct bin_attribute *attr; /* descriptor for sysfs VPD entry */
};

extern int pci_vpd_pci22_init(struct pci_dev *dev);
static inline void pci_vpd_release(struct pci_dev *dev)
{
	if (dev->vpd)
		dev->vpd->ops->release(dev);
}

/* PCI /proc functions */
#ifdef CONFIG_PROC_FS
extern int pci_proc_attach_device(struct pci_dev *dev);
extern int pci_proc_detach_device(struct pci_dev *dev);
extern int pci_proc_detach_bus(struct pci_bus *bus);
#else
static inline int pci_proc_attach_device(struct pci_dev *dev) { return 0; }
static inline int pci_proc_detach_device(struct pci_dev *dev) { return 0; }
static inline int pci_proc_detach_bus(struct pci_bus *bus) { return 0; }
#endif

/* Functions for PCI Hotplug drivers to use */
extern unsigned int pci_do_scan_bus(struct pci_bus *bus);

extern void pci_remove_legacy_files(struct pci_bus *bus);

/* Lock for read/write access to pci device and bus lists */
extern struct rw_semaphore pci_bus_sem;

extern unsigned int pci_pm_d3_delay;

#ifdef CONFIG_PCI_MSI
void pci_no_msi(void);
extern void pci_msi_init_pci_dev(struct pci_dev *dev);
#else
static inline void pci_no_msi(void) { }
static inline void pci_msi_init_pci_dev(struct pci_dev *dev) { }
#endif

#ifdef CONFIG_PCIEAER
void pci_no_aer(void);
#else
static inline void pci_no_aer(void) { }
#endif

static inline int pci_no_d1d2(struct pci_dev *dev)
{
	unsigned int parent_dstates = 0;

	if (dev->bus->self)
		parent_dstates = dev->bus->self->no_d1d2;
	return (dev->no_d1d2 || parent_dstates);

}
extern int pcie_mch_quirk;
extern struct device_attribute pci_dev_attrs[];
extern struct device_attribute dev_attr_cpuaffinity;

/**
 * pci_match_one_device - Tell if a PCI device structure has a matching
 *                        PCI device id structure
 * @id: single PCI device id structure to match
 * @dev: the PCI device structure to match against
 *
 * Returns the matching pci_device_id structure or %NULL if there is no match.
 */
static inline const struct pci_device_id *
pci_match_one_device(const struct pci_device_id *id, const struct pci_dev *dev)
{
	if ((id->vendor == PCI_ANY_ID || id->vendor == dev->vendor) &&
	    (id->device == PCI_ANY_ID || id->device == dev->device) &&
	    (id->subvendor == PCI_ANY_ID || id->subvendor == dev->subsystem_vendor) &&
	    (id->subdevice == PCI_ANY_ID || id->subdevice == dev->subsystem_device) &&
	    !((id->class ^ dev->class) & id->class_mask))
		return id;
	return NULL;
}

struct pci_dev *pci_find_upstream_pcie_bridge(struct pci_dev *pdev);

/* PCI slot sysfs helper code */
#define to_pci_slot(s) container_of(s, struct pci_slot, kobj)

extern struct kset *pci_slots_kset;

struct pci_slot_attribute {
	struct attribute attr;
	ssize_t (*show)(struct pci_slot *, char *);
	ssize_t (*store)(struct pci_slot *, const char *, size_t);
};
#define to_pci_slot_attr(s) container_of(s, struct pci_slot_attribute, attr)


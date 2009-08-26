#include <linux/kernel.h>

int
workqueue_ktask(void *arg)
{
	printk("In workqueue_ktask\n");
	printk("Bringing up Linux drivers.\n");
	driver_init_by_name("linux", "*");
	printk("Done bringing up Linux drivers.\n");

	while (1) {
		run_workqueues();
		schedule();
	}
}

void
linux_init(void)
{
        extern int postcore_initcall_pcibus_class_init(void);
        extern int postcore_initcall_pci_driver_init(void);
        extern int arch_initcall_pci_access_init(void);
        extern int subsys_initcall_pcibios_init(void);
        extern int subsys_initcall_pci_legacy_init(void);
        extern int subsys_initcall_pcibios_irq_init(void);
        extern int fs_initcall_pcibios_assign_resources(void);
        extern int fs_initcall_pci_iommu_init(void);
        extern int device_initcall_pci_init(void);
        extern int core_initcall_filelock_init(void);

        extern void init_workqueues(void);
        extern int early_param_iommu_setup(void);
        extern int early_param_pci_setup(void);
        extern void driver_init(void );
        void unnamed_dev_init(void);
        int fs_initcall_anon_inode_init(void);
        extern void vfs_caches_init(unsigned long);
        extern int core_initcall_filelock_init(void);
        extern int module_init_ramfs_fs(void);

        init_workqueues();
        driver_init();
        early_param_pci_setup();

        int rc;

        rc = postcore_initcall_pcibus_class_init();

        rc = postcore_initcall_pci_driver_init();

        rc = arch_initcall_pci_arch_init();

        rc = subsys_initcall_pci_subsys_init();

	// Watchout... this might cause problems
        rc = fs_initcall_pcibios_assign_resources();

	rc = late_initcall_pci_sysfs_init();

        rc = device_initcall_pci_init();


	struct task_struct *wq = kthread_create(workqueue_ktask, NULL, "workqueue_cpu%d", this_cpu);
	if (!wq)
		panic("Failed to create Linux workqueue thread.\n");
}

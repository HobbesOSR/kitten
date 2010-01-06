#include <linux/kernel.h>
#include <linux/radix-tree.h>

extern void init_workqueues(void);
extern void run_workqueues(void);
extern void driver_init(void);
extern int early_param_pci_setup(void);
extern int postcore_initcall_pcibus_class_init(void);
extern int postcore_initcall_pci_driver_init(void);
extern int arch_initcall_pci_arch_init(void);
extern int subsys_initcall_pci_subsys_init(void);
extern int fs_initcall_pcibios_assign_resources(void);
extern int late_initcall_pci_sysfs_init(void);
extern int device_initcall_pci_init(void);

int
linux_kthread(void *arg)
{
	while (1) {
		/* TODO: should only wakeup when there is work to do */
		run_workqueues();
		schedule();
	}
}

void
linux_init(void)
{
	radix_tree_init();
	init_workqueues();
	driver_init();
	early_param_pci_setup();
	postcore_initcall_pcibus_class_init();
	postcore_initcall_pci_driver_init();
	arch_initcall_pci_arch_init();
	subsys_initcall_pci_subsys_init();
	fs_initcall_pcibios_assign_resources();
	late_initcall_pci_sysfs_init();
	device_initcall_pci_init();
	driver_init_by_name("linux", "*");

	struct task_struct *tux = kthread_create(
		linux_kthread,
		NULL,
		"linux_kthread/%d",
		this_cpu
	);

	if (!tux)
		panic("Failed to create Linux kernel thread.\n");
}

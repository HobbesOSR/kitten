#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/radix-tree.h>
#include <lwk/kfs.h>

extern void driver_init(void);
extern int early_param_pci_setup(void);
extern int postcore_initcall_pcibus_class_init(void);
extern int postcore_initcall_pci_driver_init(void);
extern int arch_initcall_pci_arch_init(void);
extern int subsys_initcall_misc_init(void);
extern int subsys_initcall_pci_subsys_init(void);
extern int fs_initcall_pcibios_assign_resources(void);
extern int late_initcall_pci_sysfs_init(void);
extern int device_initcall_pci_init(void);
extern void __init chrdev_init(void);
extern void network_init(void);

static struct semaphore linux_sem = __SEMAPHORE_INITIALIZER(linux_sem, 0);

struct in_mem_priv_data {
	char* buf;
	loff_t	offset;
};


extern struct kfs_fops in_mem_fops;
extern void mkfifo( char*, mode_t );
void
linux_init(void)
{
	radix_tree_init();
extern void init_tasklets(void);
	init_tasklets();
	driver_init();
	early_param_pci_setup();
	postcore_initcall_pcibus_class_init();
	postcore_initcall_pci_driver_init();
	arch_initcall_pci_arch_init();
	chrdev_init();
	subsys_initcall_pci_subsys_init();
	subsys_initcall_misc_init();
	fs_initcall_pcibios_assign_resources();
	late_initcall_pci_sysfs_init();
	device_initcall_pci_init();

	driver_init_by_name("linux", "*");
}

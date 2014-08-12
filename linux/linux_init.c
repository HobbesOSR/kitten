#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/radix-tree.h>
#include <lwk/kfs.h>

extern void driver_init(void);
extern int postcore_initcall_pcibus_class_init(void);
extern int postcore_initcall_pci_driver_init(void);
extern int subsys_initcall_misc_init(void);
extern int subsys_initcall_pci_subsys_init(void);
extern int fs_initcall_pcibios_assign_resources(void);
extern int late_initcall_pci_sysfs_init(void);
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
	printk("Initializing Radix Tree\n");
	radix_tree_init();
	printk("initializing tasklets\n");
extern void init_tasklets(void);
	init_tasklets();
	printk("initializing Driver model\n");
	driver_init();

	printk("postcore_initcall_pcibus_class_init()\n");
	postcore_initcall_pcibus_class_init();
	printk("postcore_initcall_pci_driver_init()\n");
	postcore_initcall_pci_driver_init();
	printk("chrdev_init()\n");
	chrdev_init();
	printk("subsys_initcall_pci_subsys_init();\n");
	subsys_initcall_pci_subsys_init();
	printk("subsys_initcall_misc_init();\n");
	subsys_initcall_misc_init();
	printk("fs_initcall_pcibios_assign_resources();\n");
	fs_initcall_pcibios_assign_resources();
	printk("late_initcall_pci_sysfs_init();\n");
	late_initcall_pci_sysfs_init();

	driver_init_by_name("linux", "*");
}

#include <lwk/driver.h>
#include <lwk/kfs.h>
#include <lwk/aspace.h>
#include <lwk/kthread.h>
#include <lwk/sched.h>
#include <lwk/rcr/rcr.h>
#include "rcr_priv.h"


static int
rcr_open(struct inode *inode, struct file *file)
{
	return 0;
}


static int
rcr_release(struct inode * inode, struct file  *file)
{
	return 0;
}


static long
rcr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}


static int
rcr_mmap(struct file *file, struct vm_area_struct *vma)
{
	size_t  blackboard_size   = RCRblackboard_size();
	kaddr_t blackboard_kvaddr = (kaddr_t) RCRblackboard_base();
	paddr_t blackboard_paddr  = __pa(blackboard_kvaddr);

	int status = aspace_map_pmem(current->aspace->id, blackboard_paddr, vma->vm_start, blackboard_size);
	if (status) {
		printk(KERN_ERR "Failed to map RCR blackboard into user-space.\n");
		return -1;
	}

	return 0;
}


static struct kfs_fops
rcr_fops = {
	.open           = rcr_open,
	.release        = rcr_release,
	.unlocked_ioctl = rcr_ioctl,
	.compat_ioctl   = rcr_ioctl,
	.mmap           = rcr_mmap,
};


/*
 * This is the kernel thread that periodically updates the blackboard.
 * The set of performance counters monitoried is currently hard-coded.
 * At some point down the road, it may make sense to make the set of
 * counters monitored and the polling interval configurable via
 * ioctl's on /dev/rcr.
 */
static int
rcr_poller(void *data)
{
	int status;

	/* **************************************************************** */

	uint64_t msr_rapl_power_unit       = 0;
	uint64_t rapl_time_unit            = 0;
	uint64_t rapl_energy_unit          = 0;
	uint64_t rapl_power_unit           = 0;

	uint64_t rapl_s0_energy            = 0;
	uint64_t rapl_s0_energy_prev       = 0;
	uint64_t rapl_s0_energy_wrap_count = 0;

	/* **************************************************************** */

	/* Pointers into the blackboard where to store the performance
	 * counter values that are read by this kthread. */
	uint64_t *bb_s0_energy_ptr;
	uint64_t *bb_s0_timestamp_ptr;

	/* **************************************************************** */

	status = rdmsrl_safe(MSR_RAPL_POWER_UNIT, &msr_rapl_power_unit);
	BUG_ON(status);

	rapl_time_unit   = (msr_rapl_power_unit >> 16) & 0xF;
	rapl_energy_unit = (msr_rapl_power_unit >>  8) & 0xF;
	rapl_power_unit  = (msr_rapl_power_unit >>  0) & 0xF;

	printk(KERN_INFO "MSR RAPL Power Unit: 0x%lx (time=%lx energy=%lx power=%lx)\n",
		(unsigned long) msr_rapl_power_unit,
		(unsigned long) rapl_time_unit,
		(unsigned long) rapl_energy_unit,
		(unsigned long) rapl_power_unit
	);

	/* **************************************************************** */

	/* Allocate a slot in the blackboard for 'Socket 0 RAPL Energy' */
	status = RCRblackboard_assignSocketMeter(0, 0, ENERGY_STATUS);
	BUG_ON(status == false);

	/* Allocate a slot in the blackboard for 'Socket 0 Timestamp Counter' */
	status = RCRblackboard_assignSocketMeter(0, 0, TSC);
	BUG_ON(status == false);

	/* **************************************************************** */

	/* Get the address of the 'Socket 0 RAPL Energy' slot */
	bb_s0_energy_ptr = (uint64_t *)RCRblackboard_getSocketMeter(ENERGY_STATUS, 0, 0);
	BUG_ON(bb_s0_energy_ptr == NULL);
	printk(KERN_INFO "RCRd: bb_s0_energy_ptr = %p\n", bb_s0_energy_ptr);

	/* Get the address of the 'Socket 0 Timestamp Counter' slot */
	bb_s0_timestamp_ptr = (uint64_t *)RCRblackboard_getSocketMeter(TSC, 0, 0);
	BUG_ON(bb_s0_timestamp_ptr == NULL);
	printk(KERN_INFO "RCRd: bb_s0_timestamp_ptr = %p\n", bb_s0_timestamp_ptr);

	/* **************************************************************** */

	/* Loop forever, updating the blackboard each time through the loop */
	while (1) {

		/* ************** Socket 0 Energy Counter ***************** */

		status = rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &rapl_s0_energy);
		BUG_ON(status);

		/* Detect wrap around */
		if (rapl_s0_energy < rapl_s0_energy_prev)
			++rapl_s0_energy_wrap_count;
		rapl_s0_energy_prev = rapl_s0_energy;

		/* Update the blackboard. Return (1/1024)'ths of a Joule. */
		*bb_s0_energy_ptr = ((rapl_s0_energy + (rapl_s0_energy_wrap_count << 32))) / (1 << (rapl_energy_unit - 10));

		/* ************** Socket 0 Timestamp Counter ************** */

		status = rdmsrl_safe(MSR_IA32_TSC, bb_s0_timestamp_ptr);
		BUG_ON(status);

		/* ******************************************************** */

		//printk("RCRd: bb_s0_energy    = %lu\n", (unsigned long) *bb_s0_energy_ptr);
		//printk("RCRd: bb_s0_timestamp = %lu\n", (unsigned long) *bb_s0_timestamp_ptr);

		/* ******************************************************** */

		/* Go to sleep for awhile */
		schedule_timeout(NSEC_PER_SEC/10);
	}

	return 0;
}


/*
 * Called with RCR driver is loaded at LWK boot.
 * This initializes RCR.
 */
static int
rcr_init(void)
{
	kfs_create(RCR_DEV_PATH, NULL, &rcr_fops, 0777, NULL, 0);

	/* Setup the blackboard.
	 * There is one blackboard for the entire node.
	 * All user-level processes share the same blackboard. */
	RCRblackboard_buildSharedMemoryBlackBoard(
		/* num system meters */ 0,
		/* num of nodes */      1,
		/* num node meters */   0,
		/* num sockets */       1,
		/* num socket meters */ 2,
		/* num cores */         60,
		/* num core meters */   0,
		/* num threads */       4,
		/* num thread meters */ 0
	);

	/* Kick-off the kthread that updates the blackboard */
	kthread_run(rcr_poller, NULL, "RCR");

	printk("RCR Driver Loaded\n");
	return 0;
}


DRIVER_INIT("kfs", rcr_init);

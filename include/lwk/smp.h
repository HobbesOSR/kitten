#ifndef __LINUX_SMP_H
#define __LINUX_SMP_H

#ifdef __KERNEL__

/*
 *	Generic SMP support
 *		Alan Cox. <alan@redhat.com>
 */

#include <lwk/compiler.h>
#include <arch/smp.h>

/*
 * main cross-CPU interfaces, handles INIT, TLB flush, STOP, etc.
 * (defined in asm header):
 */ 

/*
 * stops all CPUs but the current one:
 */
extern void smp_send_stop(void);

/*
 * sends a 'reschedule' event to another CPU:
 */
extern void smp_send_reschedule(int cpu);


/*
 * Prepare machine for booting other CPUs.
 */
extern void smp_prepare_cpus(unsigned int max_cpus);

/*
 * Bring a CPU up
 */
extern int __cpu_up(unsigned int cpunum);

/*
 * Final polishing of CPUs
 */
extern void smp_cpus_done(unsigned int max_cpus);

/*
 * Call a function on all processors
 */
int on_each_cpu(void (*func) (void *info), void *info, int wait);

#define MSG_ALL_BUT_SELF	0x8000	/* Assume <32768 CPU's */
#define MSG_ALL			0x8001

#define MSG_INVALIDATE_TLB	0x0001	/* Remote processor TLB invalidate */
#define MSG_STOP_CPU		0x0002	/* Sent to shut down slave CPU's
					 * when rebooting
					 */
#define MSG_RESCHEDULE		0x0003	/* Reschedule request from master CPU*/
#define MSG_CALL_FUNCTION       0x0004  /* Call function on all other CPUs */

/*
 * Mark the boot cpu "online" so that it can call console drivers in
 * printk() and can access its per-cpu storage.
 */
void smp_prepare_boot_cpu(void);

#define smp_processor_id() raw_smp_processor_id()
#define get_cpu()		smp_processor_id()
#define put_cpu()		do { } while (0)
#define put_cpu_no_resched()	do { } while (0)

/**
 * Returns the current CPU's logical ID.
 */
#define this_cpu smp_processor_id()

void __init arch_boot_cpu(unsigned int cpu);
void __init cpu_init(void);

#endif

int phys_cpu_add(unsigned int phys_cpu_id, unsigned int apic_id);
int phys_cpu_remove(unsigned int phys_cpu_id, unsigned int apic_id);


#endif /* _LWK_SMP_H */

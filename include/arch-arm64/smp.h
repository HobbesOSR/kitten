#ifndef _ASM_SMP_H
#define _ASM_SMP_H

/* Values for secondary_data.status */

#define CPU_MMU_OFF		(-1)
#define CPU_BOOT_SUCCESS	(0)
/* The cpu invoked ops->cpu_die, synchronise it with cpu_kill */
#define CPU_KILL_ME		(1)
/* The cpu couldn't die gracefully and is looping in the kernel */
#define CPU_STUCK_IN_KERNEL	(2)
/* Fatal system error detected by secondary CPU, crash the system */
#define CPU_PANIC_KERNEL	(3)


/*
 * We need the APIC definitions automatically as part of 'smp.h'
 */
#ifndef __ASSEMBLY__
/* #include <linux/threads.h>s */
#include <lwk/cpumask.h>
#include <lwk/bitops.h>
#endif

#ifndef __ASSEMBLY__
#include <arch/fixmap.h>
//#include <asm/mpspec.h>
//#include <asm/io_apic.h>
//#include <asm/thread_info.h>
#include <arch/pda.h>

struct pt_regs;


/*
 * Called from C code, this handles an IPI.
 */
extern void handle_IPI(int ipinr, struct pt_regs *regs);

/*
 * Discover the set of possible CPUs and determine their
 * SMP operations.
 */
extern void smp_init_cpus(void);

/*
 * Provide a function to raise an IPI cross call on CPUs in callmap.
 */
extern void set_smp_cross_call(void (*)(const struct cpumask *, unsigned int));

extern void (*__smp_cross_call)(const struct cpumask *, unsigned int);
/*
 * Called from the secondary holding pen, this is the secondary CPU entry point.
 */
asmlinkage void secondary_start_kernel(void);

/*
 * Initial data for bringing up a secondary CPU.
 * @stack  - sp for the secondary CPU
 * @status - Result passed back from the secondary CPU to
 *           indicate failure.
 */
struct secondary_data {
	void *stack;
	struct task_struct *task;
	long status;
};

extern struct secondary_data secondary_data;
extern long __early_cpu_boot_status;
extern void secondary_entry(void);







extern cpumask_t cpu_present_mask;
extern cpumask_t cpu_possible_map;
extern cpumask_t cpu_online_map;
extern cpumask_t cpu_callout_map;
extern cpumask_t cpu_initialized;

#if 0

/*
 * Private routines/data
 */
 
extern void smp_alloc_memory(void);
extern volatile unsigned long smp_invalidate_needed;
extern void lock_ipi_call_lock(void);
extern void unlock_ipi_call_lock(void);
extern int smp_num_siblings;
extern void smp_send_reschedule(int cpu);
void smp_stop_cpu(void);
extern int smp_call_function_single(int cpuid, void (*func) (void *info),
				void *info, int retry, int wait);







extern cpumask_t cpu_sibling_map[NR_CPUS];
extern cpumask_t cpu_core_map[NR_CPUS];
extern uint16_t phys_proc_id[NR_CPUS];
extern uint16_t cpu_core_id[NR_CPUS];
extern uint16_t cpu_llc_id[NR_CPUS];


#endif

static inline int num_booting_cpus(void)
{
	return cpus_weight(cpu_callout_map);
}

#define raw_smp_processor_id() read_pda(cpunumber)


extern int safe_smp_processor_id(void);
extern int __cpu_disable(void);
extern void __cpu_die(unsigned int cpu);
extern void prefill_possible_map(void);
extern unsigned num_processors;
extern unsigned disabled_cpus;

#endif /* !ASSEMBLY */

#define NO_PROC_ID		0xFF		/* No processor magic marker */


#include <lwk/task.h>


#define cpu_physical_id(cpu) panic("cpu_physical_id() not implemented\n");

#endif


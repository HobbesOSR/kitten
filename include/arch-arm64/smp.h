#ifndef _ASM_SMP_H
#define _ASM_SMP_H

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

extern cpumask_t cpu_present_mask;
extern cpumask_t cpu_possible_map;
extern cpumask_t cpu_online_map;
extern cpumask_t cpu_callout_map;
extern cpumask_t cpu_initialized;

/*
 * Private routines/data
 */
 
extern void smp_alloc_memory(void);
extern volatile unsigned long smp_invalidate_needed;
extern int pic_mode;
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

#define SMP_TRAMPOLINE_BASE 0x6000

/*
 * On x86 all CPUs are mapped 1:1 to the APIC space.
 * This simplifies scheduling and IPI sending and
 * compresses data structures.
 */

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


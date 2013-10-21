#ifndef __ASM_APIC_H
#define __ASM_APIC_H

#include <lwk/init.h>
#include <arch/fixmap.h>
#include <arch/apicdef.h>
#include <arch/system.h>

/*
 * Debugging macros
 */
#define APIC_QUIET   0
#define APIC_VERBOSE 1
#define APIC_DEBUG   2

extern int apic_verbosity;
extern int apic_runs_main_timer;

extern unsigned long lapic_phys_addr;

/*
 * Define the default level of output to be very little
 * This can be turned up by using apic=verbose for more
 * information and apic=debug for _lots_ of information.
 * apic_verbosity is defined in apic.c
 */
#define apic_printk(v, s, a...) do {       \
		if ((v) <= apic_verbosity) \
			printk(s, ##a);    \
	} while (0)

struct pt_regs;

/*
 * Basic functions accessing APICs.
 */

static __inline void apic_write(unsigned long reg, uint32_t val)
{
	*((volatile uint32_t *)(APIC_BASE+reg)) = val;
}

static __inline uint32_t apic_read(unsigned long reg)
{
	return *((volatile uint32_t *)(APIC_BASE+reg));
}

static inline void lapic_ack_interrupt(void)
{
	/*
	 * This gets compiled to a single instruction:
	 * 	movl   $0x0,0xffffffffffdfe0b0
	 *
	 * Docs say use 0 for future compatibility.
	 */
	apic_write(APIC_EOI, 0);
}

extern void clear_local_APIC (void);
extern void connect_bsp_APIC (void);
extern void disconnect_bsp_APIC (int virt_wire_setup);
extern void disable_local_APIC (void);
extern int verify_local_APIC (void);
extern void cache_APIC_registers (void);
extern void sync_Arb_IDs (void);
extern void init_bsp_APIC (void);
extern void setup_local_APIC (void);
extern void init_apic_mappings (void);
extern void smp_local_timer_interrupt (struct pt_regs * regs);
extern void setup_boot_APIC_clock (void);
extern void setup_secondary_APIC_clock (void);
extern void setup_apic_nmi_watchdog (void);
extern int reserve_lapic_nmi(void);
extern void release_lapic_nmi(void);
extern void disable_timer_nmi_watchdog(void);
extern void enable_timer_nmi_watchdog(void);
extern void nmi_watchdog_tick (struct pt_regs * regs, unsigned reason);
extern int APIC_init_uniprocessor (void);
extern void disable_APIC_timer(void);
extern void enable_APIC_timer(void);
extern void clustered_apic_check(void);

extern void nmi_watchdog_default(void);
extern int setup_nmi_watchdog(char *);

extern unsigned int nmi_watchdog;
#define NMI_DEFAULT	-1
#define NMI_NONE	0
#define NMI_IO_APIC	1
#define NMI_LOCAL_APIC	2
#define NMI_INVALID	3

extern int disable_timer_pin_1;

extern void setup_threshold_lvt(unsigned long lvt_off);

void smp_send_timer_broadcast_ipi(void);
void switch_APIC_timer_to_ipi(void *cpumask);
void switch_ipi_to_APIC_timer(void *cpumask);

#define ARCH_APICTIMER_STOPS_ON_C3	1

extern unsigned boot_cpu_id;

extern void __init lapic_map(void);
extern void __init lapic_init(void);
extern void __init lapic_stop_timer(void);
extern void lapic_set_timer_freq(unsigned int hz);
extern unsigned int lapic_calibrate_timer(void);
extern void lapic_dump(void);
extern void lapic_send_init_ipi(unsigned int cpu);
extern void lapic_send_startup_ipi(unsigned int cpu, unsigned long start_rip);
extern void lapic_send_ipi(unsigned int cpu, unsigned int vector);
extern void lapic_send_ipi_to_apic(unsigned int apic_id, unsigned int vector);

#endif /* __ASM_APIC_H */

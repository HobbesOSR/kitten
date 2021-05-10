#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/time.h>
#include <arch/io.h>

#ifdef CONFIG_PC
/**
 * Lock that synchronizes access to the Programmable Interval Timer.
 */
static DEFINE_SPINLOCK(pit_lock);

/**
 * This stops the Programmable Interval Timer's periodic system timer
 * (channel 0). Some systems, BOCHS included, are booted with the PIT system
 * timer enabled. The LWK doesn't use the PIT, so this function is used during
 * bootstrap to disable it.
 */
static void __init
pit_stop_timer0(void)
{
	printk("TIMER NOT INITED\n");
#if 0
	unsigned long flags;
	unsigned PIT_MODE = 0x43;
	unsigned PIT_CH0  = 0x40;

	spin_lock_irqsave(&pit_lock, flags);
	outb_p(0x30, PIT_MODE);		/* mode 0 */
	outb_p(0, PIT_CH0);		/* LSB system timer interval */
	outb_p(0, PIT_CH0);		/* MSB system timer interval */
	spin_unlock_irqrestore(&pit_lock, flags);
#endif
}

/**
 * This uses the Programmable Interval Timer that is standard on all
 * PC-compatible systems to determine the time stamp counter frequency.
 *
 * This uses the speaker output (channel 2) of the PIT. This is better than
 * using the timer interrupt output because we can read the value of the
 * speaker with just one inb(), where we need three i/o operations for the
 * interrupt channel. We count how many ticks the TSC does in 50 ms.
 *
 * Returns the detected time stamp counter frequency in KHz.
 */
static unsigned int __init
pit_calibrate_tsc(void)
{
	printk("PIT NOT INIT\n");
#if 0
	cycles_t start, end;
	unsigned long flags;
	unsigned long pit_tick_rate = 1193182UL;  /* 1.193182 MHz */

	spin_lock_irqsave(&pit_lock, flags);

	outb((inb(0x61) & ~0x02) | 0x01, 0x61);

	outb(0xb0, 0x43);
	outb((pit_tick_rate / (1000 / 50)) & 0xff, 0x42);
	outb((pit_tick_rate / (1000 / 50)) >> 8, 0x42);
	start = get_cycles_sync();
	while ((inb(0x61) & 0x20) == 0);
	end = get_cycles_sync();

	spin_unlock_irqrestore(&pit_lock, flags);
	
	return (end - start) / 50;
#endif
}
#endif



void __init
time_init(void)
{
	unsigned int cpu_khz;
	unsigned int lapic_khz;

	/*
	 * Detect the CPU frequency
	 */
#if defined CONFIG_PC
	cpu_khz = pit_calibrate_tsc();
	pit_stop_timer0();
#else
	#warning "In time_init(), unknown system architecture."
#endif

	struct arch_cpuinfo * const arch_info = &cpu_info[this_cpu].arch;
	arch_info->cur_cpu_khz = cpu_khz;
	arch_info->max_cpu_khz = cpu_khz;
	arch_info->min_cpu_khz = cpu_khz;
	arch_info->tsc_khz     = cpu_khz;

	init_cycles2ns(cpu_khz);

	/*
	 * Detect the Local APIC timer's base clock frequency
	 */
	if (this_cpu == 0) {
//		lapic_khz = lapic_calibrate_timer();
		printk("TODO: Calibrate the periodic Timer\n");

	} else {
		lapic_khz = cpu_info[0].arch.lapic_khz;
	}

	arch_info->lapic_khz   = lapic_khz;

	printk(KERN_DEBUG "CPU %u: %u.%03u MHz, LAPIC bus %u.%03u MHz\n",
		this_cpu,
		cpu_khz / 1000, cpu_khz % 1000,
		lapic_khz / 1000, lapic_khz % 1000
	);
}


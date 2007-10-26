#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <arch/io.h>
#include <arch/tsc.h>

/**
 * Lock that synchronizes access to the Programmable Interval Timer.
 */
DEFINE_SPINLOCK(pit_lock);

/**
 * This uses the Programmable Interval Timer that is standard on all
 * PC-compatible systems to determine the time stamp counter frequency.
 *
 * This uses the speaker output (channel 2) of the PIT. This is better than
 * using the timer interrupt output, because we can read the value of the
 * speaker with just one inb(), where we need three i/o operations for the
 * interrupt channel. We count how many ticks the TSC does in 50 ms.
 *
 * Returns the detected time stamp counter frequency in KHz.
 */
static unsigned int __init
pit_calibrate_tsc(void)
{
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
}

/**
 * The Cray XT platform does not have any real time clocks. Therefore,
 * we have to inspect various MSRs to determine the CPU frequency and
 * trust that it is accurate.
 *
 * Returns the detected CPU frequency in KHz.
 *
 * NOTE: This function should only be used on Cray XT3 and XT4 platforms.
 *       While it will work on (some) AMD Opteron K8 and K10 systems, using a
 *       timer based mechanism to detect the actual CPU frequency is preferred.
 */
static unsigned int __init
crayxt_detect_cpu_freq(void)
{
	unsigned int MHz = 200;
	unsigned int lower, upper;
	int amd_family = cpu_info[cpu_id()].arch.x86_family;
	int amd_model  = cpu_info[cpu_id()].arch.x86_model;

	if (amd_family == 16) {
		unsigned int fid; /* current frequency id */
		unsigned int did; /* current divide id */

		rdmsr(MSR_K10_COFVID_STATUS, lower, upper);
		fid = lower & 0x3f;
		did = (lower >> 6) & 0x3f;
		MHz = 100 * (fid + 0x10) / (1 << did);

	} else if (amd_family == 15) {
		unsigned int fid; /* current frequency id */

		if (amd_model < 16) {
			/* Revision C and earlier */
			rdmsr(MSR_K8_HWCR, lower, upper);
			fid = (lower >> 24) & 0x3f;
		} else {
			/* Revision D and later */
			rdmsr(MSR_K8_FIDVID_STATUS, lower, upper);
			fid = lower & 0x3f;
		}

		switch (fid) {
			case 0:   MHz *= 4; break;
			case 2:   MHz *= 5; break;
			case 4:   MHz *= 6; break;
			case 6:   MHz *= 7; break;
			case 8:   MHz *= 8; break;
			case 10:  MHz *= 9; break;
			case 12:  MHz *= 10; break;
			case 14:  MHz *= 11; break;
			case 16:  MHz *= 12; break;
			case 18:  MHz *= 13; break;
			case 20:  MHz *= 14; break;
			case 22:  MHz *= 15; break;
			case 24:  MHz *= 16; break;
			case 26:  MHz *= 17; break;
			case 28:  MHz *= 18; break;
			case 30:  MHz *= 19; break;
			case 32:  MHz *= 20; break;
			case 34:  MHz *= 21; break;
			case 36:  MHz *= 22; break;
			case 38:  MHz *= 23; break;
			case 40:  MHz *= 24; break;
			case 42:  MHz *= 25; break;
		}
	} else {
		panic("Unknown AMD CPU family (%d).", amd_family);
	}

	return (MHz * 1000); /* return CPU freq. in KHz */
}

void __init
time_init(void)
{
	unsigned int cpu_khz;

	cpu_khz = pit_calibrate_tsc();

	cpu_info[cpu_id()].arch.cur_freq = cpu_khz;

	printk(KERN_DEBUG "CPU %u frequency = %u KHz\n",
		cpu_id(), cpu_info[cpu_id()].arch.cur_freq);
}


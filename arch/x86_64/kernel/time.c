#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/time.h>
#include <arch/io.h>
#include <arch/apic.h>

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
	unsigned long flags;
	unsigned PIT_MODE = 0x43;
	unsigned PIT_CH0  = 0x40;

	spin_lock_irqsave(&pit_lock, flags);
	outb_p(0x30, PIT_MODE);		/* mode 0 */
	outb_p(0, PIT_CH0);		/* LSB system timer interval */
	outb_p(0, PIT_CH0);		/* MSB system timer interval */
	spin_unlock_irqrestore(&pit_lock, flags);
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
#endif

#ifdef CONFIG_CRAY_GEMINI

/*
 * Gemini systems have an HPET but no PIT.
 * We grab the CPU and TSC time stamp counter frequencies from MSRs,
 * rather than trying to use the HPET to calibrate.
 */
#define HWCR_BIT_FID    24            /* right bit of FID field */
#define FIDVID_STATUS_BIT_FID 0x0     /* location of current FID */
#define MSR_COFVID_STATUS 0xc0010071  /* Family 0x10 CPU (cf. 31116 BKDG */
#define MSR_PSTATE_0 0xc0010064
#define COF(FID,DID) ((100 * ((FID) + 0x10)) / (1<<(DID)))
#define GETFID(STATUS) ((STATUS) & 0x3f)
#define GETDID(STATUS) (((STATUS) >> 6) & 0x3)

/*
 * Returns the TSC frequency that we trust Coldstart has set in MSR_PSTATE_0.
 * For G34 processors, this may be different than the frequency in COFVID.
 * For earlier Family 10 processors the TSC and CPU frequency will be
 * the same.
 */
static unsigned long cray_fam10_calibrate_tsc(void)
{
	unsigned int lower, upper;
	int MHz;

	rdmsr(MSR_PSTATE_0, lower, upper);
	MHz = COF(GETFID(lower),GETDID(lower));
	return MHz;
}

static unsigned long cray_fam10_cofvid_status(void)
{
	unsigned int lower, upper;
	int MHz;

	rdmsr(MSR_COFVID_STATUS, lower, upper);
	MHz = COF(GETFID(lower),GETDID(lower));
	return MHz;
}

/**
 * Returns the detected CPU frequency in KHz.
 */
static unsigned int __init
cray_detect_cpu_freq(void)
{
	unsigned int MHz = 200;
	unsigned int lower, upper;
	int amd_family = cpu_info[this_cpu].arch.x86_family;
	int amd_model  = cpu_info[this_cpu].arch.x86_model;

	if (amd_family == 0x15) {
		/*
		 * Family15 (Orochi) processors use the
		 * same calculation as fam10.  However,
		 * CPB configuration affects which PState
		 * is "s/w P0", therefore read the cofvid
		 * status which will be s/w Pstate 0 if
		 * Coldstart setups up, but doesn't enable
		 * CPB.
		 */
		MHz = cray_fam10_cofvid_status();
	} else if (amd_family == 0x10) {
		MHz = cray_fam10_calibrate_tsc();
	} else if (amd_family == 0xf) {
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
#endif

#ifdef CONFIG_PISCES

#include <arch/pisces/pisces_boot_params.h>

extern struct pisces_boot_params * pisces_boot_params;

static unsigned int __init
pisces_detect_cpu_freq(void)
{    
    return pisces_boot_params->cpu_khz;
}


#endif

void 
arch_set_timer_freq(unsigned int hz){
	lapic_set_timer_freq(hz);
}

void 
arch_set_timer_oneshot(unsigned int nsec){
	lapic_set_timer_oneshot(nsec);
}

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
#elif defined CONFIG_CRAY_GEMINI
	cpu_khz = cray_detect_cpu_freq();
#elif defined CONFIG_PISCES
	cpu_khz = pisces_detect_cpu_freq();
#else
	#error "In time_init(), unknown system architecture."
#endif

	struct arch_cpuinfo * const arch_info = &cpu_info[this_cpu].arch;

	/* TODO: Fix this -- we're currently assuming that cpu_khz
	 *       is the same as the tsc_khz, and that cpu_khz never
	 *       changes. */
	arch_info->cur_cpu_khz = cpu_khz;
	arch_info->max_cpu_khz = cpu_khz;
	arch_info->min_cpu_khz = cpu_khz;
	arch_info->tsc_khz     = cpu_khz;

	init_cycles2ns(cpu_khz);

	/*
	 * Detect the Local APIC timer's base clock frequency
	 */
	if (this_cpu == 0) {
		lapic_khz = lapic_calibrate_timer();
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


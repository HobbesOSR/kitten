#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/time.h>
#include <arch/io.h>
#include <arch/pda.h>

#include <arch/intc.h>

void __init
time_init(void)
{
	uint32_t cpu_khz = (u32)mrs(CNTFRQ_EL0) / 1000;
	/*
	 * Cache the CPU frequency
	 */

	struct arch_cpuinfo * const arch_info = &cpu_info[this_cpu].arch;
	
	printk("TODO: Check if the CPU clock rate has been passed in via Device Tree\n");
	
	/* 
	if (of_property_read_u32(np, "clock-frequency", &arch_timer_rate))
		arch_timer_rate = rate;
	*/


	arch_info->cur_cpu_khz = cpu_khz;
	arch_info->tsc_khz     = cpu_khz;



	init_cycles2ns(cpu_khz);


	printk(KERN_DEBUG "CPU %u: %u.%03u MHz\n",
		this_cpu,
		cpu_khz / 1000, cpu_khz % 1000
	);
}


struct cntp_ctl_el0 {
    union {
	uint64_t val;
	struct {
	    u64 enabled    : 1;
	    u64 irq_mask   : 1;
	    u64 irq_status : 1;
	    u64 res0       : 61;
	};
    };
} __attribute__((packed));

//CNTPCT_EL0
//CNTP_CVAL_EL0
//CNTP_TVAL_EL0
//CNTP_CTL_EL0
//Poll CTP_CTL_EL0
void
timer_set_freq(unsigned int hz)
{
	struct cntp_ctl_el0 cntp_ctl_el0 = {mrs(CNTP_CTL_EL0)};

	uint32_t cpu_hz    = (u32)mrs(CNTFRQ_EL0);
	uint64_t reset_val = cpu_hz / (hz);
	printk("Setting timer frequency to %d HZ\n", hz);
	printk("Setting Timer countdown value to %d\n", reset_val);

	printk("CTL initial value=%x\n", cntp_ctl_el0.val);

	cntp_ctl_el0.irq_mask = 0;
	cntp_ctl_el0.enabled  = 0;
	msr(CNTP_CTL_EL0, cntp_ctl_el0.val);

	enable_irq(30, IRQ_EDGE_TRIGGERED);

	msr(CNTP_TVAL_EL0, reset_val);

	write_pda(timer_reload_value, reset_val);


	{
		uint64_t cur_val = mrs(CNTPCT_EL0);
		uint64_t tval    = mrs(CNTP_TVAL_EL0);
		uint64_t cval    = mrs(CNTP_CVAL_EL0);

		printk("cur_val = %lld, tval=%lld, cval=%lld\n", cur_val, tval, cval);
	}

	cntp_ctl_el0.enabled = 1;
	msr(CNTP_CTL_EL0, cntp_ctl_el0.val);

	printk("Timer Enabled and running\n");

	local_irq_enable();

	while (1) {
		static uint64_t x = 0;

		if ((x % 200000000) == 0) {
			uint64_t cur_val = mrs(CNTPCT_EL0);
			uint64_t tval    = mrs(CNTP_TVAL_EL0);
			uint64_t cval    = mrs(CNTP_CVAL_EL0);
			uint64_t ctl     = mrs(CNTP_CTL_EL0);

			printk("cur_val = %lld, tval=%lld, cval=%lld ctl=%x\n", cur_val, tval, cval, ctl);
			probe_pending_irqs();

		}

		x++;
	}


	return;
}
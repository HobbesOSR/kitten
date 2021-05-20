#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/time.h>
#include <lwk/interrupt.h>
#include <arch/io.h>
#include <arch/pda.h>

#include <arch/intc.h>
#include <arch/time.h>
#include <arch/of.h>





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


static void 
__reload_timer()
{
	uint64_t curr_cnt  = mrs(CNTPCT_EL0);
	uint64_t reset_val = 0;

	reset_val = read_pda(timer_reload_value);

	printk("Reloading Timer with %lld at %lld\n", reset_val, curr_cnt);
	msr(CNTP_TVAL_EL0, reset_val);
}

static irqreturn_t 
__timer_tick(int irq, void * dev_id)
{

	printk("Tick!\n");

	msr(CNTP_CTL_EL0, 0);

	expire_timers();

	// This causes schedule() to be called right before
	// the next return to user-space
	set_bit(TF_NEED_RESCHED_BIT, &current->arch.flags);

	__reload_timer();

	msr(CNTP_CTL_EL0, 1);

	return IRQ_HANDLED;

}


void
arch_core_timer_init()
{
	irq_request(30, __timer_tick, 0, "timer", NULL);
	enable_irq(30, IRQ_EDGE_TRIGGERED);
}

//CNTPCT_EL0
//CNTP_CVAL_EL0
//CNTP_TVAL_EL0
//CNTP_CTL_EL0
//Poll CTP_CTL_EL0
void
arch_set_timer_freq(unsigned int hz)
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


	msr(CNTP_TVAL_EL0, reset_val);
	write_pda(timer_reload_value, reset_val);

	cntp_ctl_el0.enabled = 1;
	msr(CNTP_CTL_EL0, cntp_ctl_el0.val);

	printk("Timer Enabled and running\n");



	return;
}


static void
__arm_timer_init( struct device_tree * dt_node )
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




static const struct of_device_id timer_of_match[]  = {
	{ .compatible = "arm,armv8-timer",	.data = __arm_timer_init},
	{ .compatible = "arm,armv7-timer",	.data = __arm_timer_init},
	{},
};



void __init
time_init(void)
{
	struct device_node  * dt_node    = NULL;
	struct of_device_id * matched_np = NULL;
	timer_init_fn init_fn;

	dt_node = of_find_matching_node_and_match(NULL, timer_of_match, &matched_np);

	if (!dt_node || !of_device_is_available(dt_node))
		panic("Could not find timer\n");

	init_fn = (timer_init_fn)(matched_np->data);

	return init_fn(dt_node);
}

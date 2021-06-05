#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/time.h>
#include <lwk/interrupt.h>
#include <arch/io.h>
#include <arch/pda.h>

#include <arch/irqchip.h>
#include <arch/time.h>
#include <arch/of.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>


#define IRQ_IDX_SECURE     0
#define IRQ_IDX_NONSECURE  1
#define IRQ_IDX_VIRTUAL    2
#define IRQ_IDX_HYPERVISOR 3
/* 
 - interrupts : Interrupt list for secure, non-secure, virtual and hypervisor timers, in that order.
  */
static struct irq_def timer_irqs[4];

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


static void
__armv8_timer_core_init()
{
	irq_request(timer_irqs[IRQ_IDX_NONSECURE].vector, __timer_tick, 0, "timer", NULL);
	irqchip_enable_irq(timer_irqs[IRQ_IDX_NONSECURE].vector, timer_irqs[IRQ_IDX_NONSECURE].mode);
}

//CNTPCT_EL0
//CNTP_CVAL_EL0
//CNTP_TVAL_EL0
//CNTP_CTL_EL0
//Poll CTP_CTL_EL0
static void
__armv8_timer_set_timer_freq(unsigned int hz)
{
	struct cntp_ctl_el0 cntp_ctl_el0 = {mrs(CNTP_CTL_EL0)};

	uint32_t cpu_hz    = (u32)mrs(CNTFRQ_EL0);
	uint64_t reset_val = cpu_hz / (hz * 100);

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



	while (1) {
		printk("Timer Loop Check\n");
		printk("Current Val : %ld\n", mrs(CNTPCT_EL0));
		printk("Remaining   : %ld\n", mrs(CNTP_TVAL_EL0));

		irqchip_dump_state();

		mdelay(1000);
	}


	return;
}

static struct arch_timer armv8_timer = {
	.name               = "ARMv8",
	.dt_node            = NULL,
	.core_init          = __armv8_timer_core_init,
	.set_timer_freq     = __armv8_timer_set_timer_freq
};


void
armv8_timer_init( struct device_tree * dt_node )
{
	uint32_t cpu_khz  = (u32)mrs(CNTFRQ_EL0) / 1000;

	int i   = 0;
	int ret = 0;


	struct arch_cpuinfo * const arch_info = &cpu_info[this_cpu].arch;

	ret = parse_fdt_irqs(dt_node, 4, timer_irqs);

	if (ret != 0) {
		panic("Could not parse Timer IRQ info\n");
	}

	
	/*
	 * Cache the CPU frequency
	 */
	arch_info->cur_cpu_khz = cpu_khz;
	arch_info->tsc_khz     = cpu_khz;



	init_cycles2ns(cpu_khz);


	printk(KERN_DEBUG "CPU %u: %u.%03u MHz\n",
		this_cpu,
		cpu_khz / 1000, cpu_khz % 1000
	);

	armv8_timer.dt_node = dt_node;
	arch_timer_register(&armv8_timer);
}


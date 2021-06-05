#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/time.h>
#include <lwk/interrupt.h>
#include <arch/io.h>
#include <arch/pda.h>


#include <arch/time.h>
#include <arch/of.h>


DEFINE_PER_CPU(struct arch_timer, arch_timer);
//static struct arch_timer * arch_timer = NULL;


extern int armv7_timer_init(struct device_node * dt_node);
extern int armv8_timer_init(struct device_node * dt_node);

static const struct of_device_id timer_of_match[]  = {
	{ .compatible = "arm,armv8-timer",	.data = armv8_timer_init},
	{ .compatible = "arm,armv7-timer",	.data = armv7_timer_init},
	{},
};


void __init
time_init(void)
{
	struct device_node  * dt_node    = NULL;
	struct of_device_id * matched_np = NULL;
	arch_timer_init_fn init_fn;

	dt_node = of_find_matching_node_and_match(NULL, timer_of_match, &matched_np);

	if (!dt_node || !of_device_is_available(dt_node))
		panic("Could not find timer\n");

	init_fn = (arch_timer_init_fn)(matched_np->data);

	return init_fn(dt_node);
}



int 
arch_timer_register(struct arch_timer * timer)
{
	struct arch_timer * cpu_timer = &__get_cpu_var(arch_timer);

	if (!cpu_timer) {
		panic("Failed to register arch timer. Already registered\n");
	}

	printk("Registering Arch Timer [%s]\n", timer->name);
	*cpu_timer = *timer;
}



void arch_set_timer_freq(unsigned int hz)
{
	struct arch_timer * cpu_timer = &__get_cpu_var(arch_timer);
	ASSERT(cpu_timer->set_timer_freq != NULL);
	return cpu_timer->set_timer_freq(hz);

}


void 
arch_set_timer_oneshot(unsigned int nsec)
{
	struct arch_timer * cpu_timer = &__get_cpu_var(arch_timer);
	ASSERT(cpu_timer->set_timer_oneshot != NULL);
	return cpu_timer->set_timer_oneshot(nsec);
}


void 
arch_core_timer_init()
{
	struct arch_timer * cpu_timer = &__get_cpu_var(arch_timer);
	ASSERT(cpu_timer->core_init != NULL);
	return cpu_timer->core_init();
}
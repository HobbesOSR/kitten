#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/smp.h>

void
init_timer(
	struct timer_list *		timer
)
{
	list_head_init(&timer->lwk_timer.link);
	timer->lwk_timer.cpu      = this_cpu;
	timer->lwk_timer.expires  = (uint64_t)timer->expires;
	timer->lwk_timer.data     = (uintptr_t)timer->data;
	timer->lwk_timer.function = timer->function;
}

int
__mod_timer(
	struct timer_list *		timer,
	unsigned long			expires
)
{
	int not_expired = 0;

	BUG_ON(!timer->function);

	not_expired = timer_del(&timer->lwk_timer);

	timer->expires = expires;

	timer->lwk_timer.cpu      = this_cpu;
	timer->lwk_timer.expires  = (uint64_t)timer->expires;
	timer->lwk_timer.data     = (uintptr_t)timer->data;
	timer->lwk_timer.function = timer->function;

	timer_add(&timer->lwk_timer);

	return not_expired;
}

int
mod_timer(
	struct timer_list *		timer,
	unsigned long			expires
)
{
	return __mod_timer(timer, expires);
}

int
del_timer_sync(
	struct timer_list *		timer
)
{
	return timer_del(&timer->lwk_timer);
}

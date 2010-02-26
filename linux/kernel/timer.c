#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/smp.h>

void
init_timer(
	struct timer_list *		timer
)
{
	list_head_init(&timer->link);
	timer->cpu      = this_cpu;
}

void
init_timer_deferrable(
	struct timer_list *timer
	)
{
	init_timer(timer);
}

int
__mod_timer(
	struct timer_list *		timer,
	unsigned long			expires
)
{
	int not_expired = 0;

	BUG_ON(!timer->function);

	not_expired = timer_del(timer);

	timer->expires = expires;

	timer->cpu      = this_cpu;
	timer->expires  = expires;

	timer_add(timer);

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
	return timer_del(timer);
}

#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/smp.h>
#include <lwk/percpu.h>

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

	not_expired = del_timer(timer);

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
	f (timer_pending(timer) && timer->expires == expires)
		return 1;

	return __mod_timer(timer, expires);
}

int
del_timer(
        struct timer_list *             timer
)
{
	int ret = 0;
	if ( timer_pending(timer) ) {
		if ( ( ret = timer_del(timer) ) ) {
			list_head_init(&timer->link);
		}
	}
	return ret;
}

int
del_timer_sync(
	struct timer_list *		timer
)
{
	return del_timer(timer);
}

unsigned long round_jiffies_relative(unsigned long j)
{
	return round_jiffies(j);
}

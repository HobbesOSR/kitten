#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/spinlock.h>
#include <lwk/smp.h>
#include <lwk/percpu.h>
#include <lwk/time.h>
#include <lwk/timer.h>
#include <lwk/sched.h>
#include <lwk/xcall.h>

static DEFINE_PER_CPU(struct timer, alarm_timer);

static void
send_alarm(uintptr_t data)
{
	struct task_struct *tsk = (struct task_struct*)data;
	sigset_add(&tsk->sigpending.sigset, SIGALRM);
}

int
sys_alarm(int alarm)
{
	struct timer *t = &per_cpu(alarm_timer, this_cpu);
	//struct timer tim;
	//struct timer *t = &tim;
	t->expires = alarm;
	t->function = send_alarm;
	t->data = (unsigned long)current;
	timer_add(t);
	return 1;
}

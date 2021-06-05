#ifndef _ARCH_ARM64_TIME_H
#define _ARCH_ARM64_TIME_H

#include <arch/tsc.h>

struct arch_timer {
	char * name;
	struct device_node * dt_node;
	int (*core_init)(void);
	void (*set_timer_freq)(unsigned int hz);
	void (*set_timer_oneshot)(unsigned int nsec);
};


typedef void (*arch_timer_init_fn)(struct device_tree *);

int arch_timer_register(struct arch_timer * timer);

void arch_set_timer_freq(unsigned int hz);
void arch_set_timer_oneshot(unsigned int nsec);
void arch_core_timer_init();

#endif

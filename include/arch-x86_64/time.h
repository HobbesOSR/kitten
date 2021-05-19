#ifndef _ARCH_x86_64_TIME_H
#define _ARCH_x86_64_TIME_H

#include <arch/tsc.h>

void arch_set_timer_freq(unsigned int hz);
void arch_set_timer_oneshot(unsigned int nsec);

extern void arch_core_timer_init();
#endif

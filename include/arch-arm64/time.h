#ifndef _ARCH_ARM64_TIME_H
#define _ARCH_ARM64_TIME_H

#include <arch/tsc.h>

void arch_set_timer_freq(unsigned int hz);
void arch_set_timer_oneshot(unsigned int nsec);

void arch_core_timer_init();

#endif

#ifndef _LWK_RANDOM_H
#define _LWK_RANDOM_H

extern void rand_init(void);
extern void rand_add_interrupt_randomness(int irq);
extern void rand_get_random_bytes(void *buf, int num_bytes);

#endif

#ifndef _LWK_RANDOM_H
#define _LWK_RANDOM_H

extern void rand_init(void);
extern void rand_add_entropy(int value);
extern void rand_get_bytes(void *buf, int num_bytes);

#endif

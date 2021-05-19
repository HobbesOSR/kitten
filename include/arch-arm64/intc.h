/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef _ARM64_INTC_H
#define _ARM64_INTC_H





extern void intc_global_init(void);

extern void  intc_local_init(void);

void probe_pending_irqs(void);


typedef enum {
	IRQ_LEVEL_TRIGGERED,
	IRQ_EDGE_TRIGGERED
} irq_trigger_mode_t;


#endif
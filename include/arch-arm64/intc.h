/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef _ARM64_INTC_H
#define _ARM64_INTC_H

typedef enum {
	IRQ_LEVEL_TRIGGERED,
	IRQ_EDGE_TRIGGERED
} irq_trigger_mode_t;

struct irqchip {
	char * name;
	struct device_node * dt_node;
	int  (*core_init)(struct device_node * dt_node);
	void (*enable_irq)(int vector, irq_trigger_mode_t mode);
	void (*disable_irq)(int vector); 
	void (*dump_state)(void);
	void (*print_pending_irqs)(void);
};

typedef int (*irqchip_init_fn)(struct device_node *); 

int register_irqchip(struct irqchip * chip);

int intc_global_init(void);
int intc_local_init(void);

void probe_pending_irqs(void);





#endif
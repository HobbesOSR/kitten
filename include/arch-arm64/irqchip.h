/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef _ARM64_IRQCHIP_H
#define _ARM64_IRQCHIP_H

typedef enum {
	IRQ_LEVEL_TRIGGERED,
	IRQ_EDGE_TRIGGERED
} irq_trigger_mode_t;



struct arch_irq {
	enum {ARCH_IRQ_IPI,
	      ARCH_IRQ_EXT} type;

	uint32_t vector;
};

struct irqchip {
	char * name;
	struct device_node * dt_node;
	int  (*core_init)(struct device_node * dt_node);
	void (*enable_irq)(uint32_t vector, irq_trigger_mode_t mode);
	void (*disable_irq)(uint32_t vector); 
	void (*do_eoi)(struct arch_irq irq);
	struct arch_irq (*ack_irq)();

	void (*send_ipi)(int target_cpu, uint32_t vector);

	int  (*parse_devtree_irqs)(struct device_node * dt_node, uint32_t num_irqs, struct irq_def * irqs);

	void (*dump_state)(void);
	void (*print_pending_irqs)(void);
};

typedef int (*irqchip_init_fn)(struct device_node *); 

int irqchip_register(struct irqchip * chip);



int irqchip_global_init(void);
int irqchip_local_init(void);




void irqchip_enable_irq(uint32_t vector, irq_trigger_mode_t mode);
void irqchip_disable_irq(uint32_t vector);

void             irqchip_do_eoi(struct arch_irq irq);
struct arch_irq  irqchip_ack_irq(void);
void             irqchip_send_ipi(int target_cpu, uint32_t vector);



struct irq_def {
	uint32_t           vector;
	irq_trigger_mode_t mode;
};

int parse_fdt_irqs(struct device_node * dt_node, 
		   uint32_t             num_irqs, 
		   struct irq_def     * irqs);

void probe_pending_irqs(void);
void irqchip_dump_state(void);


#endif
#ifndef __ARM64_BCM2835_H
#define __ARM64_BCM2835_H

#include <lwk/types.h>

/*
#define BCM2835_BASE_OFFSET                     (0xb000)
#define BCM2835_IRQ_BASIC_PENDING_OFFFSET	(BCM2835_BASE_OFFSET + 0x200)
#define BCM2835_IRQ_PENDING_OFFSET(bank)	(BCM2835_BASE_OFFSET + 0x204 + (4 * (bank)))
#define BCM2835_FIQ_CONTROL_OFFSET		(BCM2835_BASE_OFFSET + 0x20c)
#define BCM2835_ENABLE_IRQS_OFFSET(bank)	(BCM2835_BASE_OFFSET + 0x210 + (4 * (bank)))
#define BCM2835_ENABLE_BASIC_IRQS_OFFSET	(BCM2835_BASE_OFFSET + 0x218)
#define BCM2835_DISABLE_IRQS_OFFSET(bank)	(BCM2835_BASE_OFFSET + 0x21c + (4 * (bank)))
#define BCM2835_DISABLE_BASIC_IRQS_OFFSET	(BCM2835_BASE_OFFSET + 0x224)
*/

#define BCM2835_IRQ_BASIC_PENDING_OFFFSET	(0x00)
#define BCM2835_IRQ_PENDING_OFFSET(bank)	(0x04 + (4 * (bank)))
#define BCM2835_FIQ_CONTROL_OFFSET		(0x0c)
#define BCM2835_ENABLE_IRQS_OFFSET(bank)	(0x10 + (4 * (bank)))
#define BCM2835_ENABLE_BASIC_IRQS_OFFSET	(0x18)
#define BCM2835_DISABLE_IRQS_OFFSET(bank)	(0x1c + (4 * (bank)))
#define BCM2835_DISABLE_BASIC_IRQS_OFFSET	(0x24)



struct bcm2835_basic_pending {
	union{
		uint32_t val;
		struct {
			uint32_t timer         : 1;
			uint32_t mailbox       : 1;
			uint32_t doorbell_0    : 1;
			uint32_t doorbell_1    : 1;
			uint32_t gpu_0_halted  : 1;
			uint32_t gpu_1_halted  : 1;
			uint32_t ill_access_1  : 1;
			uint32_t ill_access_0  : 1;
			uint32_t reg_1_pending : 1;
			uint32_t reg_2_pending : 1;
			uint32_t gpu_irq_7     : 1;
			uint32_t gpu_irq_9     : 1;
			uint32_t gpu_irq_10    : 1;
			uint32_t gpu_irq_18    : 1;
			uint32_t gpu_irq_19    : 1;
			uint32_t gpu_irq_53    : 1;
			uint32_t gpu_irq_54    : 1;
			uint32_t gpu_irq_55    : 1;
			uint32_t gpu_irq_56    : 1;
			uint32_t gpu_irq_57    : 1;
			uint32_t gpu_irq_62    : 1;
			uint32_t rsvd          : 11;
		};
	};
} __attribute__((packed));


struct bcm2835_pending {
	union {
		uint32_t val;
		struct {
			uint8_t bits[4];
		};
	};
} __attribute__((packed));


/* FIQ Interrupt Sources */
#define BCM2835_TIMER_FIQ			(64)
#define BCM2835_MAILBOX_FIQ			(65)
#define BCM2835_DOORBELL_0_FIQ			(66)
#define BCM2835_DOORBELL_1_FIQ			(67)
#define BCM2835_GPU_0_HALTED_FIQ		(68)
#define BCM2835_GPU_1_HALTED_FIQ		(69)
#define BCM2835_ILL_ACCESS_1_FIQ		(70)
#define BCM2835_ILL_ACCESS_0_FIQ		(71)

struct bcm2835_fiq {
	union {
		uint32_t val;
		struct {
			uint32_t source : 7;
			uint32_t enable : 1;
			uint32_t rsvd   : 24;
		};
	};
} __attribute__((packed));

struct bcm2835_enable {
	union {
		uint32_t val;
		struct {
			uint8_t bits[4];
		};
	};
} __attribute__((packed));

struct bcm2835_basic_enable {
	union {
		uint32_t val;
		struct {
			uint32_t timer        : 1;
			uint32_t mailbox      : 1;
			uint32_t doorbell_0   : 1;
			uint32_t doorbell_1   : 1;
			uint32_t gpu_0_halted : 1;
			uint32_t gpu_1_halted : 1;
			uint32_t ill_access_1 : 1;
			uint32_t ill_access_0 : 1;
			uint32_t rsvd         : 24;
		};
	};
} __attribute__((packed));


struct bcm2835_disable {
	union {
		uint32_t val;
		struct {
			uint8_t bits[4];
		};
	};
} __attribute__((packed));

struct bcm2835_basic_disable {
	union {
		uint32_t val;
		struct {
			uint32_t timer        : 1;
			uint32_t mailbox      : 1;
			uint32_t doorbell_0   : 1;
			uint32_t doorbell_1   : 1;
			uint32_t gpu_0_halted : 1;
			uint32_t gpu_1_halted : 1;
			uint32_t ill_access_1 : 1;
			uint32_t ill_access_0 : 1;
			uint32_t rsvd         : 24;
		};
	};
} __attribute__((packed));
#endif
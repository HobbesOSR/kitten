
#ifndef __ARM64_BCM2836_H
#define __ARM64_BCM2836_H

#include <lwk/types.h>

#define BCM2836_CTRL_OFFSET			(0x0000)
#define BCM2836_PRESCALAR_OFFSET		(0x0008)
#define BCM2836_GPU_IRQS_OFFSET 		(0x000c)
#define BCM2836_PMU_IRQS_SET_OFFSET 	        (0x0010)
#define BCM2836_PMU_IRQS_CLR_OFFSET 	        (0x0014)
#define BCM2836_CORE_TIMER_LO_OFFSET		(0x001c)
#define BCM2836_CORE_TIMER_HI_OFFSET		(0x0020)
#define BCM2836_LOCAL_IRQS_OFFSET		(0x0024)
#define BCM2836_AXI_COUNTER_OFFSET		(0x002c)
#define BCM2836_AXI_IRQ_OFFSET			(0x0030)
#define BCM2836_TIMER_CTRL_OFFSET		(0x0034)
#define BCM2836_TIMER_FLAGS_OFFSET		(0x0038)
#define BCM2836_TIMER_IRQS_OFFSET(cpu)		(0x0040 + ((cpu) * 4))
#define BCM2836_MAILBOX_IRQS_OFFSET(cpu)	(0x0050 + ((cpu) * 4))
#define BCM2836_IRQ_SOURCE_OFFSET(cpu)		(0x0060 + ((cpu) * 4))
#define BCM2836_FIQ_SOURCE_OFFSET(cpu)		(0x0070 + ((cpu) * 4))
#define BCM2836_MAILBOX_SET_OFFSET(cpu, mbox)	(0x0080 + ((cpu) * 16) + ((mbox) * 4))
#define BCM2836_MAILBOX_CLR_OFFSET(cpu, mbox)	(0x00c0 + ((cpu) * 16) + ((mbox) * 4))


struct bcm2836_ctrl {
	union {
		uint32_t val;
		struct {
			uint32_t rsvd_0    : 8;
			uint32_t clock_src : 1;
			uint32_t timer_inc : 1;
			uint32_t rsvd_1    : 22;
		};
	};
} __attribute__((packed));


struct bcm2836_gpu_irq_routing {
	union {
		uint32_t val;
		struct {
			uint32_t irq_cpu : 2;
			uint32_t fiq_cpu : 2;
			uint32_t rsvd    : 28;

		};
	};
} __attribute__((packed));


struct bcm2836_pmu_irq_routing {
	union {
		uint32_t val;
		struct {
			uint32_t irq_0   : 1;
			uint32_t irq_1   : 1;
			uint32_t irq_2   : 1;
			uint32_t irq_3   : 1;
			uint32_t fiq_0   : 1;
			uint32_t fiq_1   : 1;
			uint32_t fiq_2   : 1;
			uint32_t fiq_3   : 1;
			uint32_t rsvd    : 24;

		};
	};
} __attribute__((packed));

struct bcm2836_timer_irqs {
	union {
		uint32_t val;
		struct {
			uint32_t cntps_irq   : 1;
			uint32_t cntpns_irq  : 1;
			uint32_t cnthp_irq   : 1;
			uint32_t cntv_irq    : 1;
			uint32_t cntps_fiq   : 1;
			uint32_t cntpns_fiq  : 1;
			uint32_t cnthp_fiq   : 1;
			uint32_t cntv_fiq    : 1;
			uint32_t rsvd        : 24;

		};
	};
} __attribute__((packed));


struct bcm2836_mailbox_ctrl {
	union {
		uint32_t val;
		struct {
			uint32_t mbox_0_irq   : 1;
			uint32_t mbox_1_irq   : 1;
			uint32_t mbox_2_irq   : 1;
			uint32_t mbox_3_irq   : 1;
			uint32_t mbox_0_fiq   : 1;
			uint32_t mbox_1_fiq   : 1;
			uint32_t mbox_2_fiq   : 1;
			uint32_t mbox_3_fiq   : 1;
			uint32_t rsvd         : 24;


		};
	};
} __attribute__((packed));


struct bcm2836_axi_counters {
	union {
		uint32_t val;
		struct {
			uint32_t outstanding_reads  : 10;
			uint32_t res0_0             : 6;
			uint32_t outstanding_writes : 10;
			uint32_t res0_1             : 6;
		};
	};
} __attribute__((packed));


struct bcm2836_axi_irqs {
	union {
		uint32_t val;
		struct {
			uint32_t timeout_ms24 : 20;
			uint32_t irq_enable   : 1;
			uint32_t rsvd         : 11;
		};
	};
} __attribute__((packed));


struct bcm2836_irq_src {
	union {
		uint32_t val;
		struct {
			uint32_t cntps       : 1;
			uint32_t cntpns      : 1;
			uint32_t cnthp       : 1;
			uint32_t cntv        : 1;
			uint32_t mbox_0      : 1;
			uint32_t mbox_1      : 1;
			uint32_t mbox_2      : 1;
			uint32_t mbox_3      : 1;
			uint32_t gpu         : 1;
			uint32_t pmu         : 1;
			uint32_t axi         : 1;
			uint32_t local_timer : 1;
			uint32_t __unused    : 6;
			uint32_t rsvd        : 4;
		};
	};
} __attribute__((packed));

struct bcm2836_fiq_src {
	union {
		uint32_t val;
		struct {
			uint32_t cntps       : 1;
			uint32_t cntpns      : 1;
			uint32_t cnthp       : 1;
			uint32_t cntv        : 1;
			uint32_t mbox_0      : 1;
			uint32_t mbox_1      : 1;
			uint32_t mbox_2      : 1;
			uint32_t mbox_3      : 1;
			uint32_t gpu         : 1;
			uint32_t pmu         : 1;
			uint32_t res0        : 1;
			uint32_t local_timer : 1;
			uint32_t __unused    : 6;
			uint32_t rsvd        : 4;
		};
	};
} __attribute__((packed));

struct bcm2836_timer_ctrl {
	union {
		uint32_t val;
		struct {
			uint32_t reload_val   : 28;
			uint32_t timer_enable : 1;
			uint32_t irq_enable   : 1;
			uint32_t __unused     : 1;
			uint32_t irq_flag     : 1;
		};
	};
} __attribute__((packed));

struct bcm2836_timer_reload {
	union {
		uint32_t val;
		struct {
			uint32_t __unused       : 28;
			uint32_t __unspecified  : 2;
			uint32_t reload_val     : 1;
			uint32_t clear_irq      : 1;
		};
	};
} __attribute__((packed));

struct bcm2836_timer_irq_routing {
	union {
		uint32_t val;
		struct {
			uint32_t cpu       : 2;
			uint32_t fiq       : 1;
			uint32_t __unused  : 29;
		};
	};
} __attribute__((packed));


struct bcm2836_pending {
	union {
		uint32_t val;
		struct {


		};
	};
} __attribute__((packed));




#endif
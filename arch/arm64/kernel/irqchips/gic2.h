#ifndef __ARM64_GIC_H
#define __ARM64_GIC_H

#include <lwk/types.h>


/* GIC Distributor register map */
#define GICD_CTLR_OFFSET             (0x0000)
#define GICD_TYPER_OFFSET            (0x0004)
#define GICD_IIDR_OFFSET             (0x0008)
#define GICD_IGROUPR_OFFSET(n)       (0x0080 + ((n) * 4))
#define GICD_ISENABLER_OFFSET(n)     (0x0100 + ((n) * 4))
#define GICD_ICENABLER_OFFSET(n)     (0x0180 + ((n) * 4))
#define GICD_ISPENDR_OFFSET(n)       (0x0200 + ((n) * 4))
#define GICD_ICPENDR_OFFSET(n)       (0x0280 + ((n) * 4))
#define GICD_ISACTIVER_OFFSET(n)     (0x0300 + ((n) * 4))
#define GICD_ICACTIVER_OFFSET(n)     (0x0380 + ((n) * 4))
#define GICD_IPRIORITYR_OFFSET(n)    (0x0400 + ((n) * 4))
#define GICD_ITARGETSR_OFFSET(n)     (0x0800 + ((n) * 4))
#define GICD_ICFGR_OFFSET(n)         (0x0c00 + ((n) * 4))
#define GICD_NSACR_OFFSET(n)         (0x0e00 + ((n) * 4))
#define GICD_PPISR_OFFSET(n)         (0x0d00)
#define GICD_SPISR_OFFSET(n)         (0x0d04 + ((n) * 4))
#define GICD_SGIR_OFFSET             (0x0f00)
#define GICD_CPENDSGIR_OFFSET(n)     (0x0f10 + ((n) * 4))
#define GICD_SPENDSGIR_OFFSET(n)     (0x0f20 + ((n) * 4))
#define GICD_PIDR4_OFFSET            (0x0fd0)
#define GICD_PIDR5_OFFSET            (0x0fd4)
#define GICD_PIDR6_OFFSET            (0x0fd8)
#define GICD_PIDR7_OFFSET            (0x0fdc)
#define GICD_PIDR0_OFFSET            (0x0fe0)
#define GICD_PIDR1_OFFSET            (0x0fe4)
#define GICD_PIDR2_OFFSET            (0x0fe8)
#define GICD_PIDR3_OFFSET            (0x0fec)
#define GICD_CIDR0_OFFSET            (0x0ff0)
#define GICD_CIDR1_OFFSET            (0x0ff4)
#define GICD_CIDR2_OFFSET            (0x0ff8)
#define GICD_CIDR3_OFFSET            (0x0ffc)



/* The GIC CPU interface register map */
// Note: These should not be used
//       We should go through the system register interface instead 
#define GICC_CTLR_OFFSET             (0x0000)
#define GICC_PMR_OFFSET              (0x0004)
#define GICC_BPR_OFFSET              (0x0008)
#define GICC_IAR_OFFSET              (0x000c)
#define GICC_EOIR_OFFSET             (0x0010)
#define GICC_RPR_OFFSET              (0x0014)
#define GICC_HPPIR_OFFSET            (0x0018)
#define GICC_ABPR_OFFSET             (0x001c)
#define GICC_AIAR_OFFSET             (0x0020)
#define GICC_AEOIR_OFFSET            (0x0024)
#define GICC_AHPPIR_OFFSET           (0x0028)
#define GICC_APR_OFFSET(n)           (0x00d0 + ((n) * 4))
#define GICC_NSAPR_OFFSET(n)         (0x00e0 + ((n) * 4))
#define GICC_IIDR_OFFSET             (0x00fc)
#define GICC_DIR_OFFSET              (0x1000)



/* GICD_CTLR, Distributor Control Register */
struct gicd_ctlr_s {
	union {
		uint32_t val;
		struct {
			uint32_t enable_grp_0   : 1;
			uint32_t enable_grp_1ns : 1;
			uint32_t enable_grp_1s  : 1;
			uint32_t res0_0         : 1;
			uint32_t are_s          : 1;
			uint32_t are_ns         : 1;
			uint32_t ds             : 1;
			uint32_t e1_nwf         : 1;
			uint32_t res0_1	        : 23;
			uint32_t rwp            : 1;
		};
	};
} __attribute__((packed));

struct gicd_ctlr_ns {
	union {
		uint32_t val;
		struct {
			uint32_t enable_grp_1   : 1;
			uint32_t enable_grp_1a  : 1;
			uint32_t res0_0         : 2;
			uint32_t are_ns         : 1;
			uint32_t res0_1	        : 26;
			uint32_t rwp            : 1;
		};
	};
} __attribute__((packed));


/* GICD_TYPER, Interrupt Controller Type Register */
struct gicd_typer {
	union {
		uint32_t val;
		struct {
			uint32_t it_lines_num  : 5;
			uint32_t cpu_num       : 3;
			uint32_t espi          : 1;
			uint32_t res0          : 1;
			uint32_t security_extn : 1;
			uint32_t num_lpis      : 5;
			uint32_t mbis          : 1;
			uint32_t lpis          : 1;
			uint32_t dvis          : 1;
			uint32_t id_bits       : 5;
			uint32_t a3v           : 1;
			uint32_t no_1n         : 1;
			uint32_t rss           : 1;
			uint32_t espi_range    : 5;
		};
	};
} __attribute__((packed));


/* GICD_IIDR, Distributor Implementer Identification Register */
struct gicd_iidr {
	union {
		uint32_t val;
		struct {
			uint32_t implementer : 12;
			uint32_t revision    : 4;
			uint32_t variant     : 4;
			uint32_t res0        : 4;
			uint32_t product_id  : 8;  // 0x2 = GIC-400
			
		};
	};
} __attribute__((packed));



/* GICD_CPENDSGIR<n>, SGI Clear-Pending Registers, n = 0 - 3 */
struct gicd_cpendsgir {
	union {
		uint32_t val;
		struct {
			uint8_t bits[4];
		};
	};
} __attribute__((packed));

/* GICD_ICACTIVER<n>, Interrupt Clear-Active Registers, n = 0 - 31 */
struct gicd_icactiver {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));


/* GICD_ICENABLER<n>, Interrupt Clear-Enable Registers, n = 0 - 31 */
struct gicd_icenabler {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));


/* GICD_ICFGR<n>, Interrupt Configuration Registers, n = 0 - 63 */
struct gicd_icfgr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICD_NSACR<n>, Non-secure Access Control Registers, n = 0 - 63 */
struct gicd_nsacr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* PPISR, Private Peripheral Interrupt Status Register */
struct gicd_ppisr {
	union {
		uint32_t val;
		struct {
			uint32_t res0_0          : 9;
			uint32_t virt_maint_irq  : 1; // Virtual maintenance interrupt.
			uint32_t hyper_timer_irq : 1; // Hypervisor timer event
			uint32_t virt_timer_irq  : 1; // Virtual timer event
			uint32_t nlegacy_fiq     : 1; // nLEGACYFIQ signal
			uint32_t s_phys_timer    : 1; // Secure physical timer event
			uint32_t ns_phys_timer   : 1; // Non-secure physical timer event
			uint32_t nlegacy_irq     : 1; // nLEGACYIRQ signal
			uint32_t res0_1          : 16;
		};
	};
} __attribute__((packed));

/* GICD_SPISR<n>, Shared Peripheral Interrupt Status Registers, n = 0 - 16 */
struct gicd_spisr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));


/* GICD_ICPENDR<n>, Interrupt Clear-Pending Registers, n = 0 - 31 */
struct gicd_icpendr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));




/* GICD_IPRIORITYR<n>, Interrupt Priority Registers, n = 0 - 254 */
struct gicd_ipriorityr {
	union {
		uint32_t val;
		struct {
			uint8_t bits[4];
		};
	};
} __attribute__((packed));

/* GICD_IROUTER<n>, Interrupt Routing Registers, n = 32 - 1019 */
struct gicd_irouter {
	union {
		uint64_t val;
		struct {
			uint64_t aff0   : 8;
			uint64_t aff1   : 8;
			uint64_t aff2   : 8;
			uint64_t res0_0 : 7;
			uint64_t mode   : 1;
			uint64_t aff3   : 8;
			uint64_t res0_1 : 24;
		};
	};
} __attribute__((packed));


/* GICD_ISACTIVER<n>, Interrupt Set-Active Registers, n = 0 - 31 */
struct gicd_isactiver {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));


/* GICD_ISENABLER<n>, Interrupt Set-Enable Registers, n = 0 - 31 */
struct gicd_isenabler {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICD_ISPENDR<n>, Interrupt Set-Pending Registers, n = 0 - 31 */
struct gicd_ispendr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));


/* GICD_ITARGETSR<n>, Interrupt Processor Targets Registers, n = 0 - 254 */
struct gicd_itargetsr {
	union {
		uint32_t val;
		struct {
			uint8_t targets_offset[4];
		};
	};
} __attribute__((packed));



/* GICD_SGIR, Software Generated Interrupt Register */
struct gicd_sgir {
	union {
		uint32_t val;
		struct {
			uint32_t intid       : 4;
			uint32_t res0_0      : 11;
			uint32_t nsatt       : 1;
			uint32_t target_list : 8;
			uint32_t list_filter : 2;
			uint32_t res0_1      : 6;
		};
	};
} __attribute__((packed));

/* GICD_SPENDSGIR<n>, SGI Set-Pending Registers, n = 0 - 3 */
struct gicd_spendsgir {
	union {
		uint32_t val;
		struct {
			uint8_t bits[4];
		};
	};
} __attribute__((packed));


/* GICD_PIDR2, Peripheral ID2 Register */
struct gicd_pidr2 {
	union {
		uint32_t val;
		struct {
			uint32_t impl_1   : 4;
			uint32_t arch_rev : 4;
			uint32_t impl_2   : 24;
		};
	};
} __attribute__((packed));





/******************
 * GICC Registers *
 ******************/

/* GICC_ABPR, CPU Interface Aliased Binary Point Register */
struct gicc_abpr {
	union {
		uint32_t val;
		struct {
			uint32_t binary_point : 3;
			uint32_t res0         : 29;
		};
	};
} __attribute__((packed));


/* GICC_AEOIR, CPU Interface Aliased End Of Interrupt Register */
struct gicc_aeoir {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 24;
			uint32_t res0  : 8;
		};
	};
} __attribute__((packed));

/* GICC_AHPPIR, CPU Interface Aliased Highest Priority Pending Interrupt Register */
struct gicc_ahppir {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 24;
			uint32_t res0  : 8;
		};
	};
} __attribute__((packed));


/* GICC_AIAR, CPU Interface Aliased Interrupt Acknowledge Register */
struct gicc_aiar {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 24;
			uint32_t res0  : 8;
		};
	};
} __attribute__((packed));


/* GICC_APR<n>, CPU Interface Active Priorities Registers, n = 0 - 3 */
struct gicc_apr {
	union {
		uint32_t val;
		// Implementation defined
	};
} __attribute__((packed));

/* GICC_BPR, CPU Interface Binary Point Register */
struct gicc_bpr {
	union {
		uint32_t val;
		struct {
			uint32_t binary_point : 3;
			uint32_t res0         : 29;			
		};
	};
} __attribute__((packed));


/* GICC_CTLR, CPU Interface Control Register */
struct gicc_ctlr {
	union {
		uint32_t val;
		struct {
			uint32_t enable_grp1      : 1;
			uint32_t res0_0           : 4;
			uint32_t fiq_byp_dis_grp1 : 1;
			uint32_t irq_byp_dis_grp1 : 1;
			uint32_t res0_1           : 2;
			uint32_t eoi_mode_ns      : 1;
			uint32_t res0_2           : 22;
		};
	};
} __attribute__((packed));


/* GICC_DIR, CPU Interface Deactivate Interrupt Register */
struct gicc_dir {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 24;
			uint32_t res0  : 8;
		};
	};
} __attribute__((packed));


/* GICC_EOIR, CPU Interface End Of Interrupt Register */
struct gicc_eoir {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 24;
			uint32_t res0  : 8;	
		};
	};
} __attribute__((packed));


/* GICC_HPPIR, CPU Interface Highest Priority Pending Interrupt Register */
struct gicc_hppir {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 24;
			uint32_t res0  : 8;
		};
	};
} __attribute__((packed));


/* GICC_IAR, CPU Interface Interrupt Acknowledge Register */
struct gicc_iar {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 24;
			uint32_t res0  : 8;
		};
	};
} __attribute__((packed));

/* GICC_IIDR, CPU Interface Identification Register */
struct gicc_iidr {
	union {
		uint32_t val;
		struct {
			uint32_t implementer  : 12;
			uint32_t revision     : 4;
			uint32_t arch_version : 4;
			uint32_t product_id   : 12;
		};
	};
} __attribute__((packed));

/* GICC_NSAPR<n>, CPU Interface Non-secure Active Priorities Registers, n = 0 - 3 */
struct gicc_nsapr {
	union {
		uint32_t val;
		// implementation defined
	};
} __attribute__((packed));

/* GICC_PMR, CPU Interface Priority Mask Register */
struct gicc_pmr {
	union {
		uint32_t val;
		struct {
			uint32_t priority : 8;
			uint32_t res0     : 24;
		};
	};
} __attribute__((packed));

/* GICC_RPR, CPU Interface Running Priority Register */
struct gicc_rpr {
	union {
		uint32_t val;
		struct {
			uint32_t priority : 8;
			uint32_t res0     : 24;
		};
	};
} __attribute__((packed));




#endif

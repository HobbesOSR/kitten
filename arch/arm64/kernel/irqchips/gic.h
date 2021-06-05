#ifndef __ARM64_GIC_H
#define __ARM64_GIC_H

#include <lwk/types.h>


/* ICC_AP0R<n>_EL1, Interrupt Controller Active Priorities Group 0 Registers, n = 0 - 3 */
struct icc_ap0r {
	union {
		uint64_t val; 
		struct {
			uint64_t impl_defined : 32;
			uint64_t res0         : 32;
		};
	};
} __attribute__((packed));


/* ICC_AP1R<n>_EL1, Interrupt Controller Active Priorities Group 1 Registers, n = 0 - 3 */
struct icc_ap1r {
	union {
		uint64_t val; 
		struct {
			uint64_t impl_defined : 32;
			uint64_t res0         : 32;
		};
	};
} __attribute__((packed));



/* ICC_ASGI1R_EL1, Interrupt Controller Alias Software Generated Interrupt Group 1 Register */
struct icc_asgi1r {
	union {
		uint64_t val; 
		struct {
			uint64_t tgt_list : 16;
			uint64_t aff1     : 8;
			uint64_t intid    : 4;
			uint64_t res0_0   : 4;
			uint64_t aff2     : 8;
			uint64_t irm      : 1;
			uint64_t res0_1   : 3;
			uint64_t rs       : 4;
			uint64_t aff3     : 8;
			uint64_t res0_2   : 8;
		};
	};
} __attribute__((packed));


/* ICC_BPR0_EL1, Interrupt Controller Binary Point Register 0 */
/* ICC_BPR1_EL1, Interrupt Controller Binary Point Register 1 */
struct icc_bpr {
	union {
		uint64_t val;
		struct {
			uint64_t binary_point  : 3;
			uint64_t res0          : 61;
		};
	};
} __attribute__((packed));




/* ICC_CTLR_EL1, Interrupt Controller Control Register (EL1) */
struct icc_ctlr {
	union {
		uint64_t val; 
		struct {
			uint64_t cbpr      : 1;
			uint64_t eoi_mode  : 1;
			uint64_t res0_0    : 4;
			uint64_t pmhe      : 1;
			uint64_t res0_1    : 1;
			uint64_t pri_bits  : 3;
			uint64_t id_bits   : 3;
			uint64_t seis      : 1;
			uint64_t a3v       : 1;
			uint64_t res0_2    : 2;
			uint64_t rss       : 1;
			uint64_t ext_range : 1;
			uint64_t res0_3    : 44;
		};
	};
} __attribute__((packed));


/* ICC_DIR_EL1, Interrupt Controller Deactivate Interrupt Register */
struct icc_dir {
	union {
		uint64_t val;
		struct {
			uint64_t intid : 24;
			uint64_t res0  : 40;
		};
	};
} __attribute__((packed));


/* ICC_EOIR0_EL1, Interrupt Controller End Of Interrupt Register 0 */
/* ICC_EOIR1_EL1, Interrupt Controller End Of Interrupt Register 1 */
struct icc_eoir {
	union {
		uint64_t val;
		struct {
			uint64_t intid : 24;
			uint64_t res0  : 40;
		};
	};
} __attribute__((packed));




/* ICC_HPPIR0_EL1, Interrupt Controller Highest Priority Pending Interrupt Register 0 */
/* ICC_HPPIR1_EL1, Interrupt Controller Highest Priority Pending Interrupt Register 1 */
struct icc_hppir {
	union {
		uint64_t val;
		struct {
			uint64_t intid : 24;
			uint64_t res0  : 40;
		};
	};
} __attribute__((packed));





/* ICC_IAR0_EL1, Interrupt Controller Interrupt Acknowledge Register 0 */
/* ICC_IAR1_EL1, Interrupt Controller Interrupt Acknowledge Register 1 */
struct icc_iar {
	union {
		uint64_t val;
		struct {
			uint64_t intid : 24;
			uint64_t res0  : 40;
		};
	};
} __attribute__((packed));




/* ICC_IGRPEN0_EL1, Interrupt Controller Interrupt Group 0 Enable register */
/* ICC_IGRPEN1_EL1, Interrupt Controller Interrupt Group 1 Enable register */
struct icc_igrpen {
	union {
		uint64_t val;
		struct {
			uint64_t enable : 1;
			uint64_t res0   : 63;
		};
	};
} __attribute__((packed));






/* ICC_PMR_EL1, Interrupt Controller Interrupt Priority Mask Register */
struct icc_pmr {
	union {
		uint64_t val;
		struct {
			uint64_t priority : 8;
			uint64_t res0     : 56;
		};
	};
} __attribute__((packed));


/* ICC_RPR_EL1, Interrupt Controller Running Priority Register */
struct icc_rpr {
	union {
		uint64_t val;
		struct {
			uint64_t priority : 8;
			uint64_t res0     : 56;
		};
	};
} __attribute__((packed));


/* ICC_SGI0R_EL1, Interrupt Controller Software Generated Interrupt Group 0 Register */
/* ICC_SGI1R_EL1, Interrupt Controller Software Generated Interrupt Group 1 Register */
struct icc_sgir {
	union {
		uint64_t val;
		struct {
			uint64_t tgt_list : 16;
			uint64_t aff1     : 8;
			uint64_t intid    : 4;
			uint64_t res0_0   : 4;
			uint64_t aff2     : 8;
			uint64_t irm      : 1;
			uint64_t res0_1   : 3;
			uint64_t rs       : 4;
			uint64_t aff3     : 8;
			uint64_t res0_2   : 8;
		};
	};
} __attribute__((packed));




/* ICC_SRE_EL1, Interrupt Controller System Register Enable register (EL1) */
struct icc_sre {
	union {
		uint64_t val; 
		struct {
			uint64_t sre  : 1;
			uint64_t dfb  : 1;
			uint64_t dib  : 1;
			uint64_t res0 : 61;
		};
	};
} __attribute__((packed));


// Structure template
#if 0
/*  */
struct icc_ {
	union {
		uint64_t val;
		struct {
			
		};
	};
} __attribute__((packed));
#endif


/* GIC Distributor register map */
#define GICD_CTLR_OFFSET             (0x0000)
#define GICD_TYPER_OFFSET            (0x0004)
#define GICD_IIDR_OFFSET             (0x0008)
#define GICD_TYPER2_OFFSET           (0x000c)
#define GICD_STATUSR_OFFSET          (0x0010)
#define GICD_SETSPI_NSR_OFFSET       (0x0040)
#define GICD_CLRSPI_NSR_OFFSET       (0x0048)
#define GICD_SETSPI_SR_OFFSET        (0x0050)
#define GICD_CLRSPI_SR_OFFSET        (0x0058)
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
#define GICD_IGRPMODR_OFFSET(n)      (0x0d00 + ((n) * 4))
#define GICD_NSACR_OFFSET(n)         (0x0e00 + ((n) * 4))
#define GICD_SGIR_OFFSET             (0x0f00)
#define GICD_CPENDSGIR_OFFSET(n)     (0x0f10 + ((n) * 4))
#define GICD_SPENDSGIR_OFFSET(n)     (0x0f20 + ((n) * 4))
#define GICD_IGROUPR_E_OFFSET(n)     (0x1000 + ((n) * 4))
#define GICD_ISENABLER_E_OFFSET(n)   (0x1200 + ((n) * 4))
#define GICD_ICENABLER_E_OFFSET(n)   (0x1400 + ((n) * 4))
#define GICD_ISPENDR_E_OFFSET(n)     (0x1600 + ((n) * 4))
#define GICD_ICPENDR_E_OFFSET(n)     (0x1880 + ((n) * 4))
#define GICD_ISACTIVER_E_OFFSET(n)   (0x1a00 + ((n) * 4))
#define GICD_ICACTIVER_E_OFFSET(n)   (0x1c80 + ((n) * 4))
#define GICD_IPRIORITYR_E_OFFSET(n)  (0x2000 + ((n) * 4))
#define GICD_ICFGR_E_OFFSET(n)       (0x3000 + ((n) * 4))
#define GICD_IGRPMODR_E_OFFSET(n)    (0x3400 + ((n) * 4))
#define GICD_NSACR_E_OFFSET(n)       (0x3600 + ((n) * 4))
#define GICD_IROUTER_OFFSET(n)       (0x6100 + ((n) * 4))
#define GICD_IROUTER_E_OFFSET(n)     (0x8000 + ((n) * 4))
#define GICD_PIDR2_OFFSET            (0xffe8)

#define GICR_CPU_OFFSET(cpu)         (0x20000 * cpu)

/* GIC physical LPI Redistributor register map */
#define GICR_CTLR_OFFSET             (0x0000)
#define GICR_IDDR_OFFSET             (0x0004)
#define GICR_TYPER_OFFSET            (0x0008)
#define GICR_STATUSR_OFFSET          (0x0010)
#define GICR_WAKER_OFFSET            (0x0014)
#define GICR_MPAMIDR_OFFSET          (0x0018)
#define GICR_PARTIDR_OFFSET          (0x001c)
#define GICR_SETLPIR_OFFSET          (0x0040)
#define GICR_CLRLPIR_OFFSET          (0x0048)
#define GICR_PROPBASER_OFFSET        (0x0070)
#define GICR_PENDBASER_OFFSET        (0x0078)
#define GICR_INVLPIR_OFFSET          (0x00a0)
#define GICR_INVALLR_OFFSET          (0x00b0)
#define GICR_SYNCR_OFFSET            (0x00c0)
#define GICR_PIDR2_OFFSET            (0xffe8)

/* GIC virtual LPI Redistributor register map */
#define GICR_VPROPBASER_OFFSET       (0x0070)
#define GICR_VPENDBASER_OFFSET       (0x0078)
#define GICR_VSGIR_OFFSET            (0x0080)
#define GICR_VSGIPENDR_OFFSET        (0x0088)

/*  GIC SGI and PPI Redistributor register map */
#define GICR_SGI_BASE                (0x10000)
#define GICR_IGROUPR_OFFSET(n)       (GICR_SGI_BASE + 0x0080 + ((n) * 4))
#define GICR_ISENABLER_OFFSET(n)     (GICR_SGI_BASE + 0x0100 + ((n) * 4))
#define GICR_ICENABLER_OFFSET(n)     (GICR_SGI_BASE + 0x0180 + ((n) * 4))
#define GICR_ISPENDR_OFFSET(n)       (GICR_SGI_BASE + 0x0200 + ((n) * 4))
#define GICR_ICPENDR_OFFSET(n)       (GICR_SGI_BASE + 0x0280 + ((n) * 4))
#define GICR_ISACTIVER_OFFSET(n)     (GICR_SGI_BASE + 0x0300 + ((n) * 4))
#define GICR_ICACTIVER_OFFSET(n)     (GICR_SGI_BASE + 0x0380 + ((n) * 4))
#define GICR_ICFGR_OFFSET(n)         (GICR_SGI_BASE + 0x0c00 + ((n) * 4))
#define GICR_IGRPMODR_OFFSET(n)      (GICR_SGI_BASE + 0x0d00 + ((n) * 4))

#define GICR_IGROUPR_0_OFFSET        (GICR_SGI_BASE + 0x0080)
#define GICR_IGROUPR_E_OFFSET(n)     (GICR_SGI_BASE + 0x0084 + ((n) * 4))
#define GICR_ISENABLER_0_OFFSET      (GICR_SGI_BASE + 0x0100)
#define GICR_ISENABLER_E_OFFSET(n)   (GICR_SGI_BASE + 0x0104 + ((n) * 4))
#define GICR_ICENABLER_0_OFFSET      (GICR_SGI_BASE + 0x0180)
#define GICR_ICENABLER_E_OFFSET(n)   (GICR_SGI_BASE + 0x0184 + ((n) * 4))
#define GICR_ISPENDR_0_OFFSET        (GICR_SGI_BASE + 0x0200)
#define GICR_ISPENDR_E_OFFSET(n)     (GICR_SGI_BASE + 0x0204 + ((n) * 4))
#define GICR_ICPENDR_0_OFFSET        (GICR_SGI_BASE + 0x0280)
#define GICR_ICPENDR_E_OFFSET(n)     (GICR_SGI_BASE + 0x0284 + ((n) * 4))
#define GICR_ISACTIVER_0_OFFSET      (GICR_SGI_BASE + 0x0300)
#define GICR_ISACTIVER_E_OFFSET(n)   (GICR_SGI_BASE + 0x0304 + ((n) * 4))
#define GICR_ICACTIVER_0_OFFSET      (GICR_SGI_BASE + 0x0380)
#define GICR_ICACTIVER_E_OFFSET(n)   (GICR_SGI_BASE + 0x0384 + ((n) * 4))
#define GICR_IPRIORITYR_OFFSET(n)    (GICR_SGI_BASE + 0x0400 + ((n) * 4))
#define GICR_IPRIORITYR_E_OFFSET(n)  (GICR_SGI_BASE + 0x0420 + ((n) * 4))
#define GICR_ICFGR_0_OFFSET          (GICR_SGI_BASE + 0x0c00)
#define GICR_ICFGR_1_OFFSET          (GICR_SGI_BASE + 0x0c04)
#define GICR_ICFGR_E_OFFSET(n)       (GICR_SGI_BASE + 0x0c08 + ((n) * 4))
#define GICR_IGRPMODR_0_OFFSET       (GICR_SGI_BASE + 0x0d00)
#define GICR_IGRPMODR_E_OFFSET(n)    (GICR_SGI_BASE + 0x0d04 + ((n) * 4))
#define GICR_NSACR_OFFSET            (GICR_SGI_BASE + 0x0e00)


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
#define GICC_STATUSR_OFFSET          (0x002c)
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
			uint32_t product_id  : 8;
			
		};
	};
} __attribute__((packed));


/* GICD_TYPER2, Interrupt Controller Type Register 2 */
struct gicd_typer2 {
	union {
		uint32_t val;
		struct {
			uint32_t vid         : 5;
			uint32_t res0_0      : 2;
			uint32_t vil         : 1;
			uint32_t n_assgi_cap : 1;
			uint32_t res0_1      : 23;
		};
	};
} __attribute__((packed));

/* GICD_STATUSR, Error Reporting Status Register */
struct gicd_statusr {
	union {
		uint32_t val;
		struct {
			uint32_t rrd  : 1;
			uint32_t wrd  : 1;
			uint32_t rwod : 1;
			uint32_t wrod : 1;
			uint32_t res0 : 28;
		};
	};
} __attribute__((packed));



/* GICD_SETSPI_NSR, Set Non-secure SPI Pending Register */
struct gicd_setspi_nsr {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 13;
			uint32_t res0  : 19;
		};
	};
} __attribute__((packed));


/* GICD_CLRSPI_NSR, Clear Non-secure SPI Pending Register */
struct gicd_clrspi_nsr {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 13;
			uint32_t res0  : 19;
		};
	};
} __attribute__((packed));


/* GICD_SETSPI_SR, Set secure SPI Pending Register */
struct gicd_setspi_sr {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 13;
			uint32_t res0  : 19;
		};
	};
} __attribute__((packed));




/* GICD_CLRSPI_SR, Clear Secure SPI Pending Register */
struct gicd_clrspi_sr {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 13;
			uint32_t res0  : 19;
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


/* GICD_ICPENDR<n>, Interrupt Clear-Pending Registers, n = 0 - 31 */
struct gicd_icpendr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));




/* GICD_IGRPMODR<n>, Interrupt Group Modifier Registers, n = 0 - 31 */
struct gicd_igrpmodr {
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


/* GICD_NSACR<n>, Non-secure Access Control Registers, n = 0 - 63 */
struct gicd_nsacr {
	union {
		uint32_t val;
		uint32_t bits;
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
 * GICM Registers *
 ******************/


/* GICM_CLRSPI_NSR, Clear Non-secure SPI Pending Register */
struct gicm_clrspi_nsr {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 13;
			uint32_t res0  : 19;
		};
	};
} __attribute__((packed));

/* GICM_CLRSPI_SR, Clear Secure SPI Pending Register */
struct gicm_clrspi_sr {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 13;
			uint32_t res0  : 19;
		};
	};
} __attribute__((packed));


/* GICM_IIDR, Distributor Implementer Identification Register */
struct gicm_iidr {
	union {
		uint32_t val;
		struct {
			uint32_t implementer : 12;
			uint32_t revision    : 4;
			uint32_t variant     : 4;
			uint32_t res0        : 4;
			uint32_t product_id  : 8;
		};
	};
} __attribute__((packed));


/* GICM_SETSPI_NSR, Set Non-secure SPI Pending Register */
struct gicm_setspi_nsr {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 13;
			uint32_t res0  : 19;
		};
	};
} __attribute__((packed));

/* GICM_SETSPI_SR, Set Secure SPI Pending Register */
struct gicm_setspi_sr {
	union {
		uint32_t val;
		struct {
			uint32_t intid : 13;
			uint32_t res0  : 19;
		};
	};
} __attribute__((packed));

/* GICM_TYPER, Distributor MSI Type Register */
struct gicm_typer {
	union {
		uint32_t val;
		struct {
			uint32_t num_spis : 11;
			uint32_t res0     : 5;
			uint32_t intid    : 13;
			uint32_t sr       : 1;
			uint32_t clr      : 1;
			uint32_t valid    : 1;
		};
	};
} __attribute__((packed));


/******************
 * GICR Registers *
 ******************/

/* GICR_CLRLPIR, Clear LPI Pending Register */
struct gicr_clrlpir {
	union {
		uint32_t val;
		struct {
			uint64_t  p_intid : 32;
			uint64_t  res0    : 32;
		};
	};
} __attribute__((packed));

/* GICR_CTLR, Redistributor Control Register */
struct gicr_ctlr {
	union {
		uint32_t val;
		struct {
			uint32_t enable_lpis : 1;
			uint32_t ces         : 1;
			uint32_t ir          : 1;
			uint32_t rwp         : 1;
			uint32_t res0_0      : 20;
			uint32_t dpg_0       : 1;
			uint32_t dpg_1ns     : 1;
			uint32_t dpg_1s      : 1;
			uint32_t res0_1      : 4;
			uint32_t uwp         : 1;
		};
	};
} __attribute__((packed));


/* GICR_ICACTIVER, Interrupt Clear-Active Register */
struct gicr_icactiver {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_ICENABLER, Interrupt Clear-Enable Register */
struct gicr_icenabler {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_ICFGR0, Interrupt Configuration Register */
struct gicr_icfgr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));


/* GICR_ICPENDR, Interrupt Clear-Pending Register */
struct gicr_icpendr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_IGROUPR0, Interrupt Group Register */
struct gicr_igroupr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_IGRPMODR0, Interrupt Group Modifier Register */
struct gicr_igrpmodr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_IIDR, Redistributor Implementer Identification Register */
struct gicr_iidr {
	union {
		uint32_t val;
		struct {
			uint32_t implementer : 12;
			uint32_t revision    : 4;
			uint32_t variant     : 4;
			uint32_t res0        : 4;
			uint32_t product_id  : 8;
		};
	};
} __attribute__((packed));


/* GICR_INVALLR, Redistributor Invalidate All Register */
struct gicr_invallr {
	union {
		uint64_t val;
		struct {
			uint64_t res0_0 : 32;
			uint64_t v_peid : 16;
			uint64_t res0_1 : 15;
			uint64_t v      : 1;
		};
	};
} __attribute__((packed));


/* GICR_INVLPIR, Redistributor Invalidate LPI Register */
struct gicr_invlpir {
	union {
		uint64_t val;
		struct {
			uint64_t intid  : 32;
			uint64_t v_peid : 16;
			uint64_t res0   : 15;
			uint64_t v      : 1;
		};
	};
} __attribute__((packed));


/* GICR_IPRIORITYR<n>, Interrupt Priority Registers, n = 0 - 7 */
struct gicr_ipriorityr {
	union {
		uint32_t val;
		struct {
			uint8_t priorities[4];
		};
	};
} __attribute__((packed));


/* GICR_ISACTIVER, Interrupt Set-Active Register */
struct gicr_isactiver {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_ISENABLER, Interrupt Set-Enable Register */
struct gicr_isenabler {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_ISPENDR, Interrupt Set-Pending Register */
struct gicr_ispendr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_MPAMIDR, Report maximum PARTID and PMG Register */
struct gicr_mpamidr {
	union {
		uint32_t val;
		struct {
			uint32_t part_id_max : 16;
			uint32_t pmg_max     : 8;
			uint32_t res0        : 8;
		};
	};
} __attribute__((packed));

/* GICR_NSACR, Non-secure Access Control Register */
struct gicr_nsacr {
	union {
		uint32_t val;
		uint32_t bits;
	};
} __attribute__((packed));

/* GICR_PARTIDR, Set PARTID and PMG Register */
struct gicr_partidr {
	union {
		uint32_t val;
		struct {
			uint32_t part_id : 16;
			uint32_t pmg     : 8;
			uint32_t res0    : 8;
		};
	};
} __attribute__((packed));


/*  GICR_PENDBASER, Redistributor LPI Pending Table Base Address Register */
struct gicr_pendbaser {
	union {
		uint64_t val;
		struct {
			uint64_t res0_0       : 7;
			uint64_t inner_cache  : 3;
			uint64_t shareability : 2;
			uint64_t res0_1       : 4;
			uint64_t phys_addr    : 36;
			uint64_t res0_2       : 4;
			uint64_t outer_cache  : 3;
			uint64_t res0_3       : 3;
			uint64_t ptz          : 1;
			uint64_t res0_4       : 1;
		};
	};
} __attribute__((packed));


/*  GICR_PROPBASER, Redistributor Properties Base Address Register */
struct gicr_propbaser {
	union {
		uint64_t val;
		struct {
			uint64_t id_bits      : 5;
			uint64_t res0_0       : 2;
			uint64_t inner_cache  : 3;
			uint64_t shareability : 2;
			uint64_t phys_addr    : 40;
			uint64_t res0_1       : 4;
			uint64_t outer_cache  : 3;
			uint64_t res0_2       : 5;
		};
	};
} __attribute__((packed));



/*  GICR_SETLPIR, Set LPI Pending Register */
struct gicr_setlpir {
	union {
		uint64_t val;
		struct {
			uint64_t p_intid : 32;
			uint64_t res0    : 32;
		};
	};
} __attribute__((packed));

/* GICR_STATUSR, Error Reporting Status Register */
struct gicr_statusr {
	union {
		uint32_t val;
		struct {
			uint32_t rrd  : 1;
			uint32_t wrd  : 1;
			uint32_t rwod : 1;
			uint32_t wrod : 1;
			uint32_t res0 : 28;
		};
	};
} __attribute__((packed));

/*  GICR_SYNCR, Redistributor Synchronize Register */
struct gicr_syncr {
	union {
		uint32_t val;
		struct {
			uint32_t busy : 1;
			uint32_t res0 : 31;
		};
	};
} __attribute__((packed));



/* GICR_TYPER, Redistributor Type Register */
struct gicr_typer {
	union {
		uint64_t val;
		struct {
			uint64_t plpis           : 1;
			uint64_t vlpis           : 1;
			uint64_t dirty           : 1;
			uint64_t direct_lpi      : 1;
			uint64_t last            : 1;
			uint64_t dpgs            : 1;
			uint64_t mpam            : 1;
			uint64_t rvpeid          : 1;
			uint64_t processor_num   : 16;
			uint64_t command_lpi_aff : 2;
			uint64_t vsgi            : 1;
			uint64_t ppi_num         : 5;
			uint64_t affinity_val    : 32;
		};
	};
} __attribute__((packed));

/* GICR_VPENDBASER, Virtual Redistributor LPI Pending Table Base Address Register */
struct gicr_vpendbaser {
	union {
		uint64_t val;
		struct {
			uint64_t res0_0        : 7;
			uint64_t inner_cache   : 3;
			uint64_t shareability  : 2;
			uint64_t res0_1        : 4;
			uint64_t physical_addr : 36;
			uint64_t res0_2        : 4;
			uint64_t outer_cache   : 3;
			uint64_t res0_3        : 1;
			uint64_t dirty         : 1;
			uint64_t pending_last  : 1;
			uint64_t idai          : 1;
			uint64_t valid         : 1;
		};
	};
} __attribute__((packed));

/* GICR_VPROPBASER, Virtual Redistributor Properties Base Address Register */
struct gicr_vpropbaser {
	union {
		uint64_t val;
		struct {
			uint64_t id_bits       : 5;
			uint64_t res0_0        : 2;
			uint64_t inner_cache   : 3;
			uint64_t shareability  : 2;
			uint64_t physical_addr : 40;
			uint64_t res0_1        : 4;
			uint64_t outer_cache   : 3;
			uint64_t res0_2        : 5;
		};
	};
} __attribute__((packed));


/* GICR_VSGIPENDR, Redistributor virtual SGI pending state register */
struct gicr_vsgipendr {
	union {
		uint32_t val;
		struct {
			uint32_t pending : 16;
			uint32_t res0    : 15;
			uint32_t busy    : 1;
		};
	};
} __attribute__((packed));


/* GICR_VSGIR, Redistributor virtual SGI pending state request register */
struct gicr_vsgir {
	union {
		uint32_t val;
		struct {
			uint32_t vpeid : 16;
			uint32_t res0  : 16;
		};
	};
} __attribute__((packed));

/* GICR_WAKER, Redistributor Wake Register */
struct gicr_waker {
	union {
		uint32_t val;
		struct {
			uint32_t impl_def_0      : 1;
			uint32_t processor_sleep : 1;
			uint32_t children_asleep : 1;
			uint32_t res0            : 28;
			uint32_t impl_def_1      : 1;
		};
	};
} __attribute__((packed));

/* GICR_PIDR2, Peripheral ID2 Register */
struct gicr_pidr2 {
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


/* GICC_STATUSR, CPU Interface Status Register */
struct gicc_statusr {
	union {
		uint32_t val;
		struct {
			uint32_t rrd  : 1;
			uint32_t wrd  : 1;
			uint32_t rwod : 1;
			uint32_t wrod : 1;
			uint32_t asv  : 1;
			uint32_t res0 : 27;
		};
	};
} __attribute__((packed));



#endif

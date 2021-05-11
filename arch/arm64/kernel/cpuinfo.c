#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/smp.h>
#include <lwk/cpuinfo.h>
#include <lwk/aspace.h>
#include <arch/processor.h>
#include <arch/proto.h>
#include <arch/mce.h>


/**
 * Information about the boot CPU.
 * The CPU capabilities stored in this structure are the lowest common
 * denominator for all CPUs in the system... in this sense, boot_cpu_data
 * is special compared to the corresponding entry in the cpu_info[] array.
 */
struct cpuinfo boot_cpu_data;


struct ccsidr_el1 {
    union {
	u64 val; 
	struct {
	    u64 line_size     : 3;
	    u64 associativity : 21;
	    u64 res0_0        : 8;
	    u64 num_sets      : 24;
	    u64 res0_1        : 8;
	};
    };
} __attribute__((packed));

struct ccsidr2_el1 {
    union {
	u64 val; 
	struct {
	    u64 num_sets    : 24;
	    u64 res0        : 40;
	};
    };
} __attribute__((packed));

struct clidr_el1 {
    union {
	u64 val; 
	struct {
	    u64 c_type_1 : 3;
	    u64 c_type_2 : 3;
	    u64 c_type_3 : 3;
	    u64 c_type_4 : 3;
	    u64 c_type_5 : 3;
	    u64 c_type_6 : 3;
	    u64 c_type_7 : 3;
	    u64 lo_uis   : 3;
	    u64 lo_c     : 3;
	    u64 lo_uu    : 3;
	    u64 icb      : 3;
	    u64 t_type_1 : 2;
	    u64 t_type_2 : 2;
	    u64 t_type_3 : 2;
	    u64 t_type_4 : 2;
	    u64 t_type_5 : 2;
	    u64 t_type_6 : 2;
	    u64 t_type_7 : 2;
	    u64 res0     : 17
	};
    };
} __attribute__((packed));

struct csselr_el1 {
    union {
	u64 val; 
	struct {
	    u64 ind     : 1; /* Instruction Not Data */
	    u64 level   : 3;
	    u64 tnd     : 1; /* Tag Not Data */
	    u64 res0    : 59;
	};
    };
} __attribute__((packed));


struct mpidr_el1 {
    union {
	u64 val; 
	struct {
	    u64 aff_0   : 8;
	    u64 aff_1   : 8;
	    u64 aff_2   : 8;
	    u64 mt      : 1;
	    u64 res0_0  : 5;
	    u64 u       : 1;  /* Uniprocessor */
	    u64 res1    : 1;
	    u64 aff_3   : 8;
	    u64 res0_1  : 24;
	};
    };
} __attribute__((packed));

static void __init
__identify_caches(struct arch_cpuinfo * a)
{
	struct clidr_el1  clidr_el1  = { mrs(CLIDR_EL1) };
	int i = 0;

	for (i = 0; i < 7; i++) {
		struct csselr_el1 csselr_el1 = { 0 }; 
		struct ccsidr_el1 ccsidr_el1 = { 0 };



		uint8_t ctype = clidr_el1.val & (0xff << (i * 3));

		csselr_el1.level = i;

		if (ctype == 0) {
			// no cache
			continue;
		}

		if (ctype == 0x4) {
			// unified cache

			csselr_el1.ind = 0;
			csselr_el1.tnd = 0;
			msr(CSSELR_EL1, csselr_el1.val);

			ccsidr_el1.val = mrs(CCSIDR_EL1);

			a->arm64_cache_size[UNIF][i] = (ccsidr_el1.num_sets + 1) * (ccsidr_el1.associativity + 1);
			a->arm64_cache_line[UNIF][i] = (16 << ccsidr_el1.line_size);
			
		} else {
			if (ctype & 0x1) {
				// icache
				csselr_el1.ind = 1;
				csselr_el1.tnd = 0;
				msr(CSSELR_EL1, csselr_el1.val);

				ccsidr_el1.val = mrs(CCSIDR_EL1);

				a->arm64_cache_line[INST][i] = (16 << ccsidr_el1.line_size);
				a->arm64_cache_size[INST][i] = (ccsidr_el1.num_sets + 1) * (ccsidr_el1.associativity + 1);
			} 
			
			if (ctype & 0x2) {
				// dcache
				csselr_el1.ind = 0;
				csselr_el1.tnd = 0;
				msr(CSSELR_EL1, csselr_el1.val);

				ccsidr_el1.val = mrs(CCSIDR_EL1);

				a->arm64_cache_line[DATA][i] = (16 << ccsidr_el1.line_size);
				a->arm64_cache_size[DATA][i] = (ccsidr_el1.num_sets + 1) * (ccsidr_el1.associativity + 1);
			}
		}


	}
}


/*
 * Do some early cpuid on the boot CPU to get some parameter that are
 * needed before check_bugs. Everything advanced is in identify_cpu
 * below.
 */
void __init
early_identify_cpu(struct cpuinfo *c)
{
	struct arch_cpuinfo *a = &c->arch;
	memset(a, 0, sizeof(*a));

	struct midr_el1         midr_el1          = { mrs(MIDR_EL1)         };
	struct mpidr_el1        mpidr_el1         = { mrs(MPIDR_EL1)        };
	struct id_aa64mmfr0_el1 id_aa64mmfr0_el1  = { mrs(ID_AA64MMFR0_EL1) };
	struct id_aa64mmfr2_el1 id_aa64mmfr2_el1  = { mrs(ID_AA64MMFR2_EL1) };

	a->arm64_vendor   = midr_el1.vendor;
	a->arm64_variant  = midr_el1.variant;
	a->arm64_arch     = midr_el1.arch;
	a->arm64_partnum  = midr_el1.partnum;
	a->arm64_revision = midr_el1.revision;


	if (a->arm64_arch != ARM64_ARCH_EXTENDED) {
		panic("HW reports a pre ARM64 architecture...");
	}

	{
		uint32_t cpu_khz = (u32)mrs(CNTFRQ_EL0) / 1000;

		a->cur_cpu_khz = cpu_khz;
		a->max_cpu_khz = cpu_khz;
		a->min_cpu_khz = cpu_khz;
		a->min_cpu_khz = cpu_khz;
		a->tsc_khz     = cpu_khz;
	}


	switch (id_aa64mmfr0_el1.pa_range) {
		case 0x0: 
			a->arm64_phys_bits = 32;
			break;
		case 0x1: 
			a->arm64_phys_bits = 36;
			break;
		case 0x2: 
			a->arm64_phys_bits = 40;
			break;
		case 0x3:
			a->arm64_phys_bits = 42;
			break;
		case 0x4:
			a->arm64_phys_bits = 44;
			break;
		case 0x5:
			a->arm64_phys_bits = 48;
			break;
		case 0x6:
			a->arm64_phys_bits = 52;
			break;
		default:
			panic("Unsupported physical address size\n");
			break;
	}

	if (id_aa64mmfr2_el1.va_range == 0) {
		a->arm64_virt_bits = 48;		
	} else {
		panic("unsupported virtual address size\n");
	}


	__identify_caches(a);


	{
		a->cpu_core_id    = mpidr_el1.aff_0;
		a->cpu_cluster_id = mpidr_el1.aff_1;
		a->cpu_affinity_2 = mpidr_el1.aff_2;
		a->cpu_affinity_3 = mpidr_el1.aff_3;

	}



	c->pagesz_mask = (VM_PAGE_4KB | VM_PAGE_2MB);
}

/*
 * This does the hard work of actually picking apart the CPU stuff...
 */
void __init
identify_cpu(void)
{
	int i;
	struct cpuinfo *c = &cpu_info[this_cpu];
	struct arch_cpuinfo *a = &c->arch;

	early_identify_cpu(c);

	print_arch_cpuinfo(c);

}


/**
 * Prints architecture specific CPU information to the console.
 */
void
print_arch_cpuinfo(struct cpuinfo *c)
{
	int i;
	struct arch_cpuinfo *a = &c->arch;
	char buf[1024];
	char * vendor_name = NULL;
	char * arch_name   = NULL;

	vendor_name = 	(a->arm64_vendor == ARM64_VENDOR_RSVD)          ? "Software"            :
		      	(a->arm64_vendor == ARM64_VENDOR_AMPERE)        ? "Ampere"              :
		     	(a->arm64_vendor == ARM64_VENDOR_ARM)           ? "ARM"                 :
		     	(a->arm64_vendor == ARM64_VENDOR_BROADCOM)      ? "Broadcom"            :
		      	(a->arm64_vendor == ARM64_VENDOR_CAVIUM)        ? "Cavium"              :
			(a->arm64_vendor == ARM64_VENDOR_DEC)           ? "DEC"                 :
			(a->arm64_vendor == ARM64_VENDOR_FUJITSU)       ? "Fujitsu"             :
			(a->arm64_vendor == ARM64_VENDOR_INFINEON)      ? "Infineon"            :
			(a->arm64_vendor == ARM64_VENDOR_FREESCALE)     ? "Motorolla/Freescale" :
			(a->arm64_vendor == ARM64_VENDOR_NVIDIA)        ? "NVidia"              :
			(a->arm64_vendor == ARM64_VENDOR_APPLIED_MICRO) ? "Applied Micro"       :
			(a->arm64_vendor == ARM64_VENDOR_QUALCOMM)      ? "Qualcomm"            :
			(a->arm64_vendor == ARM64_VENDOR_MARVELL)       ? "Marvell"             :
			(a->arm64_vendor == ARM64_VENDOR_INTEL)         ? "Intel"               : "Unknown";



	printk(KERN_DEBUG "  Vendor:         %s\n",      vendor_name);
	printk(KERN_DEBUG "  Variant:        %u\n",      a->arm64_variant);
	printk(KERN_DEBUG "  Architecture:   %u\n",      a->arm64_arch);
	printk(KERN_DEBUG "  Model:          %u.%u\n",   a->arm64_partnum, a->arm64_revision);
	printk(KERN_DEBUG "  Frequency:      %u.%03u MHz (max=%u.%03u, min=%u.%03u)\n",
	                  a->cur_cpu_khz / 1000, (a->cur_cpu_khz % 1000),
	                  a->max_cpu_khz / 1000, (a->max_cpu_khz % 1000),
	                  a->min_cpu_khz / 1000, (a->min_cpu_khz % 1000));



	for (i = 0; i < 7; i++) {
		/* Cache Info */
		if (a->arm64_cache_size[UNIF][i] == 0) {
			printk(KERN_DEBUG "  L%d Cache:       I=%u KB, line size=%u bytes ; D=%u KB, line size=%u bytes\n",
				i + 1,
				a->arm64_cache_size[INST][i],
				a->arm64_cache_line[INST][i],
				a->arm64_cache_size[DATA][i],
				a->arm64_cache_line[DATA][i]);
		} else {
			printk(KERN_DEBUG "  L%d Cache:       %u KB (unified I+D), line size=%u bytes\n",
				i + 1,
				a->arm64_cache_size[UNIF][i],
				a->arm64_cache_line[UNIF][i]);
		}
	}



#if 0

	/* 4-KB Page TLB Info */
	printk(KERN_DEBUG "  4-KB TLB:       I=%u/%u entries D=%d/%d entries\n",
		a->x86_tlb_size[INST][L1][PAGE_4KB],
		a->x86_tlb_size[INST][L2][PAGE_4KB],
		a->x86_tlb_size[DATA][L1][PAGE_4KB],
		a->x86_tlb_size[DATA][L2][PAGE_4KB]
	);

	/* 2-MB Page TLB Info */
	printk(KERN_DEBUG "  2-MB TLB:       I=%u/%u entries D=%d/%d entries\n",
		a->x86_tlb_size[INST][L1][PAGE_2MB],
		a->x86_tlb_size[INST][L2][PAGE_2MB],
		a->x86_tlb_size[DATA][L1][PAGE_2MB],
		a->x86_tlb_size[DATA][L2][PAGE_2MB]
	);

	/* 1-GB Page TLB Info */
	printk(KERN_DEBUG "  1-GB TLB:       I=%u/%u entries D=%d/%d entries\n",
		a->x86_tlb_size[INST][L1][PAGE_1GB],
		a->x86_tlb_size[INST][L2][PAGE_1GB],
		a->x86_tlb_size[DATA][L1][PAGE_1GB],
		a->x86_tlb_size[DATA][L2][PAGE_1GB]
	);

#endif

	/* Address bits */
	printk(KERN_DEBUG "  Address bits:   %u bits physical, %u bits virtual\n",
		a->arm64_phys_bits,
		a->arm64_virt_bits);

#if 0
	/* Bytes flushed by CLFLUSH instruction */
	printk(KERN_DEBUG "  CLFLUSH size:   %u bytes\n", a->x86_clflush_size);
#endif

#if 0

	/* CPU Features */
	buf[0] = '\0';
	for (i = 0; i < 32*NCAPINTS; i++) {
		if (cpu_has(c, i) && x86_cap_flags[i] != NULL) {
			strcat(buf, x86_cap_flags[i]);
			strcat(buf, " ");
		}
	}
	printk(KERN_DEBUG "  CPU Features:   %s\n", buf);

	/* Power Management Features */
	if (a->x86_power == 0) {
		strcpy(buf, "none");
	} else {
		buf[0] = '\0';
		for (i = 0; i < 32; i++) {
			if ((i < ARRAY_SIZE(x86_power_flags)) && x86_power_flags[i]) {
				strcat(buf, x86_power_flags[i]);
				strcat(buf, " ");
			} else {
				char bit_str[7];
				bit_str[0] = '\0';
				sprintf(bit_str, "[%d] ", i);
				strcat(buf, bit_str);
			}
		}
	}
	printk(KERN_DEBUG "  Power Features: %s\n", buf);

#endif
}

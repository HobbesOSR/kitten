#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/smp.h>
#include <lwk/cpuinfo.h>
#include <lwk/aspace.h>
#include <arch/processor.h>
#include <arch/proto.h>

/**
 * Information about the boot CPU.
 * The CPU capabilities stored in this structure are the lowest common
 * denominator for all CPUs in the system... in this sense, boot_cpu_data
 * is special compared to the corresponding entry in the cpu_info[] array.
 */
struct cpuinfo boot_cpu_data;

/**
 * On AMD multi-core CPUs, the lower bits of the local APIC ID distinquish the
 * cores. This function assumes the number of cores is a power of two.
 */
static void __init
amd_detect_cmp(struct cpuinfo *c)
{
	struct arch_cpuinfo *a = &c->arch;
	unsigned bits;
	unsigned ecx = cpuid_ecx(0x80000008);

	a->x86_pkg_cores = (ecx & 0xff) + 1;

	/* CPU telling us the core id bits shift? */
	bits = (ecx >> 12) & 0xF;

	/* Otherwise recompute */
	if (bits == 0) {
		while ((1 << bits) < a->x86_pkg_cores)
			bits++;
	}

	/* Determine the physical socket ID */
	c->phys_socket_id = c->physical_id >> bits;

	/* Determine the physical core ID (index of core in socket) */
	c->phys_core_id = c->physical_id & ((1 << bits)-1);
}

static void __init
amd_cpu(struct cpuinfo *c)
{
	unsigned level;
	unsigned long value;
	unsigned int eax, ebx, ecx, edx;
	struct arch_cpuinfo *a = &c->arch;

	/*
	 * Disable TLB flush filter by setting HWCR.FFDIS on K8
	 * bit 6 of msr C001_0015
 	 *
	 * Errata 63 for SH-B3 steppings
	 * Errata 122 for all steppings (F+ have it disabled by default)
	 */
	if (a->x86_family == 15) {
		rdmsrl(MSR_K8_HWCR, value);
		value |= 1 << 6;
		wrmsrl(MSR_K8_HWCR, value);
	}

	/*
	 * Bit 31 in normal CPUID used for nonstandard 3DNow ID;
	 * 3DNow is IDd by bit 31 in extended CPUID (1*32+31) anyway
	 */
	clear_bit(0*32+31, &a->x86_capability);
	
	/* On C+ stepping K8 rep microcode works well for copy/memset */
	level = cpuid_eax(1);
	if (a->x86_family == 15 && ((level >= 0x0f48 && level < 0x0f50) || level >= 0x0f58))
		set_bit(X86_FEATURE_REP_GOOD, &a->x86_capability);
	if (a->x86_family == 0x10)
		set_bit(X86_FEATURE_REP_GOOD, &a->x86_capability);

	/* Enable workaround for FXSAVE leak */
	if (a->x86_family >= 6)
		set_bit(X86_FEATURE_FXSAVE_LEAK, &a->x86_capability);

	/* Determine L1 Cache and TLB Information */
	if (a->extended_cpuid_level >= 0x80000005) {
		cpuid(0x80000005, &eax, &ebx, &ecx, &edx);

		/* 2-MB L1 TLB (inclusive with L2 TLB) */
		a->x86_tlb_size[INST][L1][PAGE_2MB] = (eax & 0xff);
		a->x86_tlb_size[DATA][L1][PAGE_2MB] = ((eax >> 16) & 0xff);

		/* 4-KB L1 TLB (inclusive with L2 TLB) */
		a->x86_tlb_size[INST][L1][PAGE_4KB] = (ebx & 0xff);
		a->x86_tlb_size[DATA][L1][PAGE_4KB] = ((ebx >> 16) & 0xff);

		/* L1 Instruction Cache */
		a->x86_cache_size[INST][L1] = (edx >> 24);
		a->x86_cache_line[INST][L1] = (edx & 0xff);

		/* L1 Data Cache */
		a->x86_cache_size[DATA][L1] = (ecx >> 24);
		a->x86_cache_line[DATA][L1] = (ecx & 0xff);
	}

	/* Determine L2 Cache and TLB Information */
	if (a->extended_cpuid_level >= 0x80000006) {
		cpuid(0x80000006, &eax, &ebx, &ecx, &edx);

		/* 2-MB L2 TLB */
		if ((eax & 0xffff0000) == 0) {
			/* Unified I+D 2-MB L2 TLB */
			a->x86_tlb_size[UNIF][L2][PAGE_2MB] = eax & 0xfff;
		} else {
			a->x86_tlb_size[INST][L2][PAGE_2MB] = eax & 0xfff;
			a->x86_tlb_size[DATA][L2][PAGE_2MB] = (eax>>16) & 0xfff;
		}

		/* 4-KB L2 TLB */
		if ((ebx & 0xffff0000) == 0) {
			/* Unified I+D 4-KB L2 TLB */
			a->x86_tlb_size[UNIF][L2][PAGE_4KB] = ebx & 0xfff;
		} else {
			a->x86_tlb_size[INST][L2][PAGE_4KB] = ebx & 0xfff;
			a->x86_tlb_size[DATA][L2][PAGE_4KB] = (ebx>>16) & 0xfff;
		}

		/* Unified L2 Cache */
		a->x86_cache_size[UNIF][L2] = ecx >> 16;
		a->x86_cache_line[UNIF][L2] = ecx & 0xff;
	}

	/* Determine Advanced Power Management Features */
	if (a->extended_cpuid_level >= 0x80000007) {
		a->x86_power = cpuid_edx(0x80000007);
	}

	/* Determine Maximum Address Sizes */
	if (a->extended_cpuid_level >= 0x80000008) {
		cpuid(0x80000008, &eax, &ebx, &ecx, &edx); 
		a->x86_virt_bits = (eax >> 8) & 0xff;
		a->x86_phys_bits = eax & 0xff;
	}

	/* a->x86_power is 8000_0007 edx. Bit 8 is constant TSC */
	if (a->x86_power & (1<<8))
		set_bit(X86_FEATURE_CONSTANT_TSC, &a->x86_capability);

	/* Multi core CPU? */
	if (a->extended_cpuid_level >= 0x80000008)
		amd_detect_cmp(c);

	if (a->x86_family == 0xf || a->x86_family == 0x10 || a->x86_family == 0x11)
		set_bit(X86_FEATURE_K8, &a->x86_capability);

	/* RDTSC can be speculated around */
	clear_bit(X86_FEATURE_SYNC_RDTSC, &a->x86_capability);

	/* Family 10 doesn't support C states in MWAIT so don't use it */
	if (a->x86_family == 0x10)
		clear_bit(X86_FEATURE_MWAIT, &a->x86_capability);
}

static void __init
intel_cpu(struct cpuinfo *c)
{
	/* TODO */
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
	uint32_t tfms;
	uint32_t misc;

	/*
 	 * Zero structure, except apic_id should have already been filled in.
 	 */
	uint8_t apic_id = a->apic_id;
	memset(a, 0, sizeof(*a));
	a->apic_id = apic_id;

	/*
 	 * Set some defaults to begin with.
 	 */
	a->x86_vendor_id[0]	=	'\0';  /* Unset */
	a->x86_model_id[0]	=	'\0';  /* Unset */
	a->x86_clflush_size	=	64;
	a->x86_pkg_cores	=	1;
	a->max_cpu_khz		=	1000000; /* start out with 1 GHz */
	a->min_cpu_khz		=	a->max_cpu_khz;
	a->cur_cpu_khz		=	a->max_cpu_khz;
	a->tsc_khz		=	a->max_cpu_khz;
	memset(&a->x86_capability, 0, sizeof(a->x86_capability));

	/* Determine the CPU vendor */
	cpuid(0x00000000, &a->cpuid_level,
	      (unsigned int *)&a->x86_vendor_id[0],
	      (unsigned int *)&a->x86_vendor_id[8],
	      (unsigned int *)&a->x86_vendor_id[4]);

	/* Derive the vendor ID from the vendor string */
	if (!strcmp(a->x86_vendor_id, "AuthenticAMD"))
		a->x86_vendor = X86_VENDOR_AMD;
	else if (!strcmp(a->x86_vendor_id, "GenuineIntel"))
		a->x86_vendor = X86_VENDOR_INTEL;
	else
		a->x86_vendor = X86_VENDOR_UNKNOWN;

	if (a->cpuid_level == 0)
		panic("CPU only has CPUID level 0... is your CPU ancient?");

	/*
	 * Determine Intel-defined CPU features and other standard info.
	 * NOTE: Vendor-specific code may override these later.
	 */
	cpuid(0x00000001,
		&tfms, /* type, family, model, stepping */
		&misc, /* brand, cflush sz, logical cpus, apic id */
		&a->x86_capability[4], /* extended cpu features */
		&a->x86_capability[0]  /* cpu features */
	);

	/* Determine the CPU family */
	a->x86_family = (tfms >> 8) & 0xf;
	if (a->x86_family == 0xf)
		a->x86_family += ((tfms >> 20) & 0xff);

	/* Determine the CPU model */
	a->x86_model = (tfms >> 4) & 0xf;
	if (a->x86_family >= 0x6)
		a->x86_model += (((tfms >> 16) & 0xf) << 4);

	/* Determine the CPU stepping */
	a->x86_stepping = tfms & 0xf;

	/* Determine the CLFLUSH size, if the CPU supports CLFLUSH */
	if (a->x86_capability[0] & (1 << X86_FEATURE_CLFLSH))
		a->x86_clflush_size = ((misc >> 8) & 0xff) * 8;

	/*
	 * Determine the CPU's initial local APIC ID.
	 * NOTE: The BIOS may change the CPU's Local APIC ID before
	 *       passing control to the OS kernel, however the value
	 *       reported by CPUID will never change. The initial APIC
	 *       ID can sometimes be used to discover CPU topology.
	 */
	a->initial_lapic_id = (misc >> 24) & 0xff;

	/* TODO: determine page sizes supported via CPUID */
	c->pagesz_mask = (VM_PAGE_4KB | VM_PAGE_2MB);
}

/*
 * This does the hard work of actually picking apart the CPU stuff...
 */
void __init
identify_cpu(void)
{
	int i;
	struct cpuinfo *c = &cpu_info[cpu_id()];
	struct arch_cpuinfo *a = &c->arch;

	early_identify_cpu(c);

	/* Determine the extended CPUID level */
	a->extended_cpuid_level = cpuid_eax(0x80000000);

	/* Parse extended CPUID information */
	if ((a->extended_cpuid_level & 0xffff0000) == 0x80000000) {
		/* Determine AMD-defined CPU features: level 0x80000001 */
		if (a->extended_cpuid_level >= 0x80000001) {
			a->x86_capability[1] = cpuid_edx(0x80000001);
			a->x86_capability[6] = cpuid_ecx(0x80000001);
		}

		/* Determine processor brand/model string */
		if (a->extended_cpuid_level >= 0x80000004) {
			unsigned int *v = (unsigned int *) a->x86_model_id;
			cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
			cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
			cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
			a->x86_model_id[48] = '\0';
		} else {
			strcpy(a->x86_model_id, "Unknown x86-64 Model");
		}
	}

	/*
	 * Vendor-specific initialization.  In this section we
	 * canonicalize the feature flags, meaning if there are
	 * features a certain CPU supports which CPUID doesn't
	 * tell us, CPUID claiming incorrect flags, or other bugs,
	 * we handle them here.
	 *
	 * At the end of this section, c->x86_capability better
	 * indicate the features this CPU genuinely supports!
	 */
	switch (a->x86_vendor) {
	case X86_VENDOR_AMD:
		amd_cpu(c);
		break;

	case X86_VENDOR_INTEL:
		intel_cpu(c);
		break;

	case X86_VENDOR_UNKNOWN:
	default:
		panic("Unknown x86 CPU Vendor.");
	}

	/*
	 * boot_cpu_data holds the common feature set between
	 * all CPUs; so make sure that we indicate which features are
	 * common between the CPUs.  The first time this routine gets
	 * executed, c == &boot_cpu_data.
	 */
	if (c != &boot_cpu_data) {
		/* AND the already accumulated flags with these */
		for (i = 0 ; i < NCAPINTS ; i++)
			boot_cpu_data.arch.x86_capability[i] &= c->arch.x86_capability[i];
	}
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

	/* 
	 * These flag bits must match the definitions in <arch/cpufeature.h>.
	 * NULL means this bit is undefined or reserved; either way it doesn't
	 * have meaning as far as the kernel is concerned.
	 */
	static char *x86_cap_flags[] = {
		/* Intel-defined */
	        "fpu", "vme", "de", "pse", "tsc", "msr", "pae", "mce",
	        "cx8", "apic", NULL, "sep", "mtrr", "pge", "mca", "cmov",
	        "pat", "pse36", "pn", "clflush", NULL, "dts", "acpi", "mmx",
	        "fxsr", "sse", "sse2", "ss", "ht", "tm", "ia64", "pbe",

		/* AMD-defined */
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, "syscall", NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, "nx", NULL, "mmxext", NULL,
		NULL, "fxsr_opt", "pdpe1gb", "rdtscp", NULL, "lm",
		"3dnowext", "3dnow",

		/* Transmeta-defined */
		"recovery", "longrun", NULL, "lrti", NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* Other (Linux-defined) */
		"cxmmx", "k6_mtrr", "cyrix_arr", "centaur_mcr",
		NULL, NULL, NULL, NULL,
		"constant_tsc", "up", NULL, "arch_perfmon",
		"pebs", "bts", NULL, "sync_rdtsc",
		"rep_good", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* Intel-defined (#2) */
		"pni", NULL, NULL, "monitor", "ds_cpl", "vmx", "smx", "est",
		"tm2", "ssse3", "cid", NULL, NULL, "cx16", "xtpr", NULL,
		NULL, NULL, "dca", NULL, NULL, NULL, NULL, "popcnt",
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* VIA/Cyrix/Centaur-defined */
		NULL, NULL, "rng", "rng_en", NULL, NULL, "ace", "ace_en",
		"ace2", "ace2_en", "phe", "phe_en", "pmm", "pmm_en", NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* AMD-defined (#2) */
		"lahf_lm", "cmp_legacy", "svm", "extapic", "cr8_legacy",
		"altmovcr8", "abm", "sse4a",
		"misalignsse", "3dnowprefetch",
		"osvw", "ibs", NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* Auxiliary (Linux-defined) */
		"ida", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	};
	static char *x86_power_flags[] = { 
		"ts",	/* temperature sensor */
		"fid",  /* frequency id control */
		"vid",  /* voltage id control */
		"ttp",  /* thermal trip */
		"tm",
		"stc",
		"100mhzsteps",
		"hwpstate",
		"",	/* tsc invariant mapped to constant_tsc */
		/* nothing */
	};

	printk(KERN_DEBUG "  Vendor:         %s\n",      a->x86_vendor_id);
	printk(KERN_DEBUG "  Family:         %u\n",      a->x86_family);
	printk(KERN_DEBUG "  Model:          %u (%s)\n", a->x86_model, a->x86_model_id);
	printk(KERN_DEBUG "  Stepping:       %u\n",      a->x86_stepping);
	printk(KERN_DEBUG "  Frequency:      %u.%03u MHz (max=%u.%03u, min=%u.%03u)\n",
	                  a->cur_cpu_khz / 1000, (a->cur_cpu_khz % 1000),
	                  a->max_cpu_khz / 1000, (a->max_cpu_khz % 1000),
	                  a->min_cpu_khz / 1000, (a->min_cpu_khz % 1000));

	/* L1 Cache Info */
	if (a->x86_cache_size[UNIF][L1] == 0) {
	printk(KERN_DEBUG "  L1 Cache:       I=%u KB, D=%u KB, line size=%u bytes\n",
	                  a->x86_cache_size[INST][L1],
	                  a->x86_cache_size[DATA][L1],
	                  a->x86_cache_line[DATA][L1]);
	} else {
	printk(KERN_DEBUG "  L1 Cache:       %u KB (unified I+D), line size=%u bytes\n",
	                  a->x86_cache_size[UNIF][L1],
	                  a->x86_cache_line[UNIF][L1]);
	}

	/* L2 Cache Info */
	if (a->x86_cache_size[UNIF][L2] == 0) {
	printk(KERN_DEBUG "  L2 Cache:       I=%u KB, D=%u KB, line size=%u bytes\n",
	                  a->x86_cache_size[INST][L2],
	                  a->x86_cache_size[DATA][L2],
	                  a->x86_cache_line[DATA][L2]);
	} else {
	printk(KERN_DEBUG "  L2 Cache:       %u KB (unified I+D), line size=%u bytes\n",
	                  a->x86_cache_size[UNIF][L2],
	                  a->x86_cache_line[UNIF][L2]);
	}

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

	/* Address bits */
	printk(KERN_DEBUG "  Address bits:   %u bits physical, %u bits virtual\n",
		a->x86_phys_bits,
		a->x86_virt_bits);

	/* Bytes flushed by CLFLUSH instruction */
	printk(KERN_DEBUG "  CLFLUSH size:   %u bytes\n", a->x86_clflush_size);

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
}


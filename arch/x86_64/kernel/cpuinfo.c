#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/smp.h>
#include <lwk/cpuinfo.h>
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
 * Gets the model name information for the calling CPU.
 */
int __cpuinit
get_model_name(struct cpuinfo_x86 *c)
{
	unsigned int *v;

	if (c->extended_cpuid_level < 0x80000004)
		return 0;

	v = (unsigned int *) c->x86_model_id;
	cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
	cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
	cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
	c->x86_model_id[48] = 0;
	return 1;
}

/*
 * Do some early cpuid on the boot CPU to get some parameter that are
 * needed before check_bugs. Everything advanced is in identify_cpu
 * below.
 */
void __cpuinit
early_identify_cpu(struct cpuinfo *c)
{
	struct arch_cpuinfo *a = &c->arch;

	a->x86_cache_size[0]	=	-1;
	a->x86_cache_size[1]	=	-1;
	a->x86_cache_size[2]	=	-1;
	a->x86_vendor		=	X86_VENDOR_UNKNOWN;
	a->x86_model		=	a->x86_mask = 0; /* So far unknown... */
	a->x86_vendor_id[0]	=	'\0';  /* Unset */
	a->x86_model_id[0]	=	'\0';  /* Unset */
	a->x86_clflush_size	=	64;
	a->x86_cache_alignment	=	a->x86_clflush_size;
	a->x86_max_cores	=	1;
	a->extended_cpuid_level	=	0;

	/* Initially zero processor capabilities. */
	memset(&a->x86_capability, 0, sizeof(a->x86_capability));

	/* Get vendor name string */
	cpuid(0x00000000, (unsigned int *)&a->cpuid_level,
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

	/* Initialize the standard set of capabilities */
	/* Note that the vendor-specific code below might override */

	/* Intel-defined flags: level 0x00000001 */
	if (a->cpuid_level >= 0x00000001) {
		uint32_t tfms;
		uint32_t misc;
		cpuid(0x00000001, &tfms, &misc, &a->x86_capability[4],
		      &a->x86_capability[0]);
		a->x86_family = (tfms >> 8) & 0xf;
		a->x86_model  = (tfms >> 4) & 0xf;
		a->x86_mask   = tfms & 0xf;
		if (a->x86_family == 0xf)
			a->x86_family += (tfms >> 20) & 0xff;
		if (a->x86_family >= 0x6)
			a->x86_model += ((tfms >> 16) & 0xF) << 4;
		if (a->x86_capability[0] & (1<<19)) 
			a->x86_clflush_size = ((misc >> 8) & 0xff) * 8;
	} else {
		/* Have CPUID level 0 only - unheard of */
		a->x86_family = 4;
	}

	c->phys_socket_id = (cpuid_ebx(1) >> 24) & 0xff;
}

#if  0
/*
 * This does the hard work of actually picking apart the CPU stuff...
 */
void __cpuinit
identify_cpu(struct cpuinfo_x86 *c)
{
	int i;
	u32 xlvl;

	early_identify_cpu(c);

	/* AMD-defined flags: level 0x80000001 */
	xlvl = cpuid_eax(0x80000000);
	c->extended_cpuid_level = xlvl;
	if ((xlvl & 0xffff0000) == 0x80000000) {
		if (xlvl >= 0x80000001) {
			c->x86_capability[1] = cpuid_edx(0x80000001);
			c->x86_capability[6] = cpuid_ecx(0x80000001);
		}
		if (xlvl >= 0x80000004)
			get_model_name(c); /* Default name */
	}

	/* Transmeta-defined flags: level 0x80860001 */
	xlvl = cpuid_eax(0x80860000);
	if ((xlvl & 0xffff0000) == 0x80860000) {
		/* Don't set x86_cpuid_level here for now to not confuse. */
		if (xlvl >= 0x80860001)
			c->x86_capability[2] = cpuid_edx(0x80860001);
	}

//	c->apicid = phys_pkg_id(0);

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
	switch (c->x86_vendor) {
	case X86_VENDOR_AMD:
		init_amd_cpuinfo(c);
		break;

	case X86_VENDOR_INTEL:
//		init_intel_cpuinfo(c);
		break;

	case X86_VENDOR_UNKNOWN:
	default:
		//amd_get_cacheinfo(c);
		while (1) {}
		break;
	}

//	select_idle_routine(c);
//	detect_ht(c); 

	/*
	 * boot_cpu_data holds the common feature set between
	 * all CPUs; so make sure that we indicate which features are
	 * common between the CPUs.  The first time this routine gets
	 * executed, c == &boot_cpu_data.
	 */
	if (c != &boot_cpu_data) {
		/* AND the already accumulated flags with these */
		for (i = 0 ; i < NCAPINTS ; i++)
			boot_cpu_data.x86_capability[i] &= c->x86_capability[i];
	}

#if 0
#ifdef CONFIG_X86_MCE
	mcheck_init(c);
#endif
	if (c == &boot_cpu_data)
		mtrr_bp_init();
	else
		mtrr_ap_init();
#ifdef CONFIG_NUMA
	numa_add_cpu(smp_processor_id());
#endif
#endif
}
#endif


/**
 * Prints architecture specific CPU information to the console.
 */
void
print_arch_cpuinfo(struct cpuinfo *c)
{
	int i;
	struct arch_cpuinfo *a = &c->arch;

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
	        "fxsr", "sse", "sse2", "ss", "ht", "tm", "ia64", NULL,

		/* AMD-defined */
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, "syscall", NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, "nx", NULL, "mmxext", NULL,
		NULL, "fxsr_opt", "rdtscp", NULL, NULL, "lm", "3dnowext", "3dnow",

		/* Transmeta-defined */
		"recovery", "longrun", NULL, "lrti", NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* Other (Linux-defined) */
		"cxmmx", NULL, "cyrix_arr", "centaur_mcr", NULL,
		"constant_tsc", NULL, "up",
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* Intel-defined (#2) */
		"pni", NULL, NULL, "monitor", "ds_cpl", "vmx", "smx", "est",
		"tm2", NULL, "cid", NULL, NULL, "cx16", "xtpr", NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* VIA/Cyrix/Centaur-defined */
		NULL, NULL, "rng", "rng_en", NULL, NULL, "ace", "ace_en",
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* AMD-defined (#2) */
		"lahf_lm", "cmp_legacy", "svm", NULL, "cr8_legacy", NULL, NULL, NULL,
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
		NULL,
		/* nothing */	/* constant_tsc - moved to flags */
	};


	printk("vendor_id\t: %s\n"
	       "cpu family\t: %d\n"
	       "model\t\t: %d\n"
	       "model name\t: %s\n",
		     a->x86_vendor_id[0] ? a->x86_vendor_id : "unknown",
		     a->x86_family,
		     (int)a->x86_model,
		     a->x86_model_id[0] ? a->x86_model_id : "unknown");
	
	if (a->x86_mask || a->cpuid_level >= 0)
		printk("stepping\t: %d\n", a->x86_mask);
	else
		printk("stepping\t: unknown\n");
	

	printk("cpu MHz\t\t: %u.%03u\n",
		a->max_freq / 1000, (a->max_freq % 1000));

	/* Cache size */
	for (i = 0; i < 3; i++) {
		if (a->x86_cache_size[i] > 0) {
			printk("L%d cache size\t: %d KB\n",
			       i, a->x86_cache_size[i]);
		}
	}

	printk( "fpu\t\t: yes\n"
	        "fpu_exception\t: yes\n"
	        "cpuid level\t: %d\n"
	        "wp\t\t: yes\n"
	        "flags\t\t:",
		   a->cpuid_level);

	for ( i = 0 ; i < 32*NCAPINTS ; i++ )
		if (cpu_has(c, i) && x86_cap_flags[i] != NULL)
			printk(" %s", x86_cap_flags[i]);
	printk("\n");
		
	/* 4 KB TLB */
	printk("4KB TLB size\t: DATA: %d/%d  INSTR: %d/%d\n",
		a->x86_tlbsize[DTLB][TLB_L1][PAGE_4KB],
		a->x86_tlbsize[DTLB][TLB_L2][PAGE_4KB],
		a->x86_tlbsize[ITLB][TLB_L1][PAGE_4KB],
		a->x86_tlbsize[ITLB][TLB_L2][PAGE_4KB]
	);

	/* 2 MB TLB */
	printk("2MB TLB size\t: DATA: %d/%d  INSTR: %d/%d\n",
		a->x86_tlbsize[DTLB][TLB_L1][PAGE_2MB],
		a->x86_tlbsize[DTLB][TLB_L2][PAGE_2MB],
		a->x86_tlbsize[ITLB][TLB_L1][PAGE_2MB],
		a->x86_tlbsize[ITLB][TLB_L2][PAGE_2MB]
	);

	/* 1 GB TLB */
	printk("2MB TLB size\t: DATA: %d/%d  INSTR: %d/%d\n",
		a->x86_tlbsize[DTLB][TLB_L1][PAGE_1GB],
		a->x86_tlbsize[DTLB][TLB_L2][PAGE_1GB],
		a->x86_tlbsize[ITLB][TLB_L1][PAGE_1GB],
		a->x86_tlbsize[ITLB][TLB_L2][PAGE_1GB]
	);

	printk("clflush size\t: %d\n", a->x86_clflush_size);
	printk("cache_alignment\t: %d\n", a->x86_cache_alignment);

	printk("address sizes\t: %u bits physical, %u bits virtual\n", 
		   a->x86_phys_bits, a->x86_virt_bits);

	printk("power management:");
	{
		unsigned i;
		for (i = 0; i < 32; i++) 
			if (a->x86_power & (1 << i)) {
				if (i < ARRAY_SIZE(x86_power_flags) &&
					x86_power_flags[i])
					printk("%s%s",
						x86_power_flags[i][0]?" ":"",
						x86_power_flags[i]);
				else
					printk(" [%d]", i);
			}
	}

	printk("\n");
}


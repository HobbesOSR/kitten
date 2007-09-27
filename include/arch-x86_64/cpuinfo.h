/**
 * Portions derived from Linux include/asm-x86_64/processor.h
 * Copyright (C) 1994 Linus Torvalds
 */

#ifndef _X86_64_CPUINFO_H
#define _X86_64_CPUINFO_H

#include <arch/cpufeature.h>

/**
 * x86_cache_size array indices.
 */
#define CACHE_L1	0
#define CACHE_L2	1
#define CACHE_L3	2

/**
 * x86_tlbsize first dimension indices.
 */
#define ITLB		0
#define DTLB		1

/**
 * x86_tlbsize second dimension indices.
 */
#define TLB_L1		0
#define TLB_L2		1

/**
 * x86_tlbsize third dimension indices.
 */
#define PAGE_4KB	0
#define PAGE_2MB	1
#define PAGE_1GB	2

/**
 * Architecture specific CPU information and hardware bug flags.
 * CPU info is kept separately for each CPU.
 */
struct arch_cpuinfo {
	uint8_t		x86;			// CPU family
	uint8_t		x86_vendor;		// CPU vendor
	uint8_t		x86_model;
	uint8_t		x86_mask;
	uint32_t	x86_capability[NCAPINTS]; // optional CPU features
	char		x86_vendor_id[16];
	char		x86_model_id[64];
	int 		x86_cache_size[3];	// in KB
	int		x86_clflush_size;	// in bytes
	int		x86_cache_alignment;	// in bytes
	int		x86_tlbsize[2][2][3];	// [I|D][LEVEL][PAGE_SIZE]
        uint8_t		x86_virt_bits;		// bits of virt address space
	uint8_t		x86_phys_bits;		// bits of phys address space
	uint8_t		x86_max_cores;		// cpuid returned max cores 
        uint32_t	x86_power;
	uint32_t	cpuid_level;		// Max supported CPUID level
	uint32_t	extended_cpuid_level;	// Max ext. CPUID func supported
	uint32_t	max_freq;		// Max CPU frequency in KHz
	uint8_t		apic_id;		// Local APIC ID
};

extern struct cpuinfo boot_cpu_info;

struct cpuinfo;
extern void print_arch_cpuinfo(struct cpuinfo *);
extern void early_identify_cpu(struct cpuinfo *);

#endif

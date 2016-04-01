/**
 * Portions derived from Linux include/asm-x86_64/processor.h
 * Copyright (C) 1994 Linus Torvalds
 */

#ifndef _X86_64_CPUINFO_H
#define _X86_64_CPUINFO_H

#include <arch/cpufeature.h>

/**
 * arch_cpuinfo.x86_cache_size first dimension indicies.
 * and
 * arch_cpuinfo.x86_tlbsize first dimension indicies.
 */
#define INST	0	/* Instruction */
#define DATA	1	/* Data */
#define UNIF	2	/* Unified Instruction and Data */

/**
 * arch_cpuinfo.x86_cache_size second dimension indices.
 * and
 * arch_cpuinfo.x86_tlbsize second dimension indices.
 */
#define L1	0
#define L2	1
#define L3	2

/**
 * arch_cpuinfo.x86_tlbsize third dimension indices.
 */
#define PAGE_4KB	0
#define PAGE_2MB	1
#define PAGE_1GB	2

/**
 * Architecture specific CPU information and hardware bug flags.
 * CPU info is kept separately for each CPU.
 */
struct arch_cpuinfo {
    uint8_t  x86_vendor;            /* CPU vendor */
    uint8_t  x86_family;            /* CPU family */
    uint8_t  x86_model;             /* CPU model */
    uint8_t  x86_stepping;          /* CPU stepping */
    uint32_t x86_capability[NCAPINTS]; /* optional CPU features */
    char     x86_vendor_id[16];     /* Vendor ID string */
    char     x86_model_id[64];      /* Model/Brand ID string */
    uint16_t x86_cache_size[3][3];  /* [I|D|U][LEVEL], in KB */
    uint16_t x86_cache_line[3][3];  /* [I|D|U][LEVEL], in bytes */
    int      x86_clflush_size;      /* In bytes */
    int      x86_cache_alignment;   /* In bytes */
    uint16_t x86_tlb_size[3][2][3]; /* [I|D|U][LEVEL][PAGE_SIZE], in #entries */
    uint8_t  x86_virt_bits;         /* Bits of virt address space */
    uint8_t  x86_phys_bits;         /* Bits of phys address space */
    uint8_t  x86_pkg_cores;         /* Number of cores in this CPU's package */
    uint32_t x86_power;             /* Power management features */
    uint32_t cpuid_level;           /* Max supported CPUID level */
    uint32_t extended_cpuid_level;  /* Max extended CPUID func supported */
    uint32_t cur_cpu_khz;           /* Current CPU freq. in KHz */
    uint32_t max_cpu_khz;           /* Maximum CPU freq. in KHz */
    uint32_t min_cpu_khz;           /* Minimum CPU freq. in KHz */
    uint32_t tsc_khz;               /* Time stamp counter freq. in KHz */
    uint32_t lapic_khz;             /* Local APIC bus freq. in KHz */
    uint32_t apic_id;               /* Local APIC ID, phys CPU ID */
    uint32_t initial_lapic_id;      /* As reported by CPU ID */
    uint16_t x86_max_cores;         /* CPUID max cores val */
};

extern struct cpuinfo boot_cpu_data;

struct cpuinfo;
extern void print_arch_cpuinfo(struct cpuinfo *);
extern void early_identify_cpu(struct cpuinfo *);

#endif

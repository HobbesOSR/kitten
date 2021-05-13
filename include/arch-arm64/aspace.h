/* Copyright (c) 2007, Sandia National Laboratories */

#ifndef _ARCH_ARM64_ASPACE_H
#define _ARCH_ARM64_ASPACE_H

#ifdef __KERNEL__
#include <arch/page_table.h>

struct arch_aspace {
	xpte_t       * pgd;	/* Page global directory... root page table */
	unsigned int   id;
};
#endif

#define SMARTMAP_ALIGN	0x8000000000UL  /* Each PML4T entry covers 512 GB */
#define SMARTMAP_SHIFT  39

#endif

/* Copyright (c) 2007, Sandia National Laboratories */

#ifndef _ARCH_X86_64_ASPACE_H
#define _ARCH_X86_64_ASPACE_H

#include <arch/page_table.h>

struct arch_aspace {
	xpte_t *pgd;	/* Page global directory... root page table */
};

#endif

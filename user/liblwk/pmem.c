/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/liblwk.h>

const char *
pmem_type_to_string(pmem_type_t type)
{
	switch(type) {
		case PMEM_TYPE_BOOTMEM: return "BOOTMEM"; break;
		case PMEM_TYPE_KMEM:    return "KERNEL";  break;
		case PMEM_TYPE_UMEM:    return "USER";    break;
	}
	return "UNKNOWN";
}

void
pmem_region_unset_all(struct pmem_region *rgn)
{
	rgn->type_is_set      = false;
	rgn->lgroup_is_set    = false;
	rgn->allocated_is_set = false;
	rgn->name_is_set      = false;
}

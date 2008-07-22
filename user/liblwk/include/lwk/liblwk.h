/* Copyright (c) 2008, Sandia National Laboratories */

#ifndef _LIBLWK_H_
#define _LIBLWK_H_

#include <lwk/types.h>
#include <lwk/errno.h>
#include <lwk/pmem.h>

/**
 * Physical memory management convenience functions.
 * See lwk/pmem.h for core API, exposed via liblwk/syscalls.c
 */
const char *pmem_type_to_string(pmem_type_t type);
void pmem_region_unset_all(struct pmem_region *rgn);

#endif

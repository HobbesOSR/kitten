/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/liblwk.h>

int
aspace_map_region(id_t id,
                  vaddr_t start, size_t extent,
                  vmflags_t flags, vmpagesize_t pagesz,
                  const char *name, paddr_t pmem)
{
	int status;

	if ((status = aspace_add_region(id, start, extent, flags, pagesz, name)))
		return status;

	if ((status = aspace_map_pmem(id, pmem, start, extent)))
		return status;

	return 0;
}

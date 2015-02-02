/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/liblwk.h>

int
aspace_map_region(
	id_t         id,
	vaddr_t      start,
	size_t       extent,
	vmflags_t    flags,
	vmpagesize_t pagesz,
	const char * name,
	paddr_t      pmem
)
{
	int status;

	if ((status = aspace_add_region(id, start, extent, flags, pagesz, name)))
		return status;

	if ((status = aspace_map_pmem(id, pmem, start, extent))) {
		aspace_del_region(id, start, extent);
		return status;
	}

	return 0;
}

int
aspace_map_region_anywhere(
	id_t         id,
	vaddr_t *    start,
	size_t       extent,
	vmflags_t    flags,
	vmpagesize_t pagesz,
	const char * name,
	paddr_t      pmem
)
{
	int status;

retry:
	if ((status = aspace_find_hole(id, 0, extent, pagesz, start)))
		return status;

	if ((status = aspace_add_region(id, *start, extent, flags, pagesz, name))) {
		if (status == -ENOTUNIQ)
			goto retry; /* we lost a race with someone */
		return status;
	}

	if ((status = aspace_map_pmem(id, pmem, *start, extent))) {
		aspace_del_region(id, *start, extent);
		return status;
	}

	return 0;
}

int 
aspace_unmap_region(
	id_t     id,
	vaddr_t  start,
	size_t   extent
)
{
	int status;
	
	if ((status = aspace_unmap_pmem(id, start, extent)))
		return status;
	
	if ((status = aspace_del_region(id, start, extent)))
		return status;
	
	return 0;
}

/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/idspace.h>
#include <lwk/log2.h>
#include <arch/page.h>

struct idspace {
	id_t   min_id;
	id_t   max_id;
	size_t size;
	size_t ids_in_use;
	size_t offset;
	void * bitmap;
};

static size_t
calc_order(struct idspace *idspace)
{
	size_t pages = DIV_ROUND_UP(idspace->size, PAGE_SIZE * 8);
	return roundup_pow_of_two(pages);
}

int
idspace_create(id_t min_id, id_t max_id, idspace_t *idspace)
{
	struct idspace *spc;

	if ((min_id == ANY_ID) || (max_id == ANY_ID))
		return -EINVAL;

	if (min_id > max_id)
		return -EINVAL;

	if (!idspace)
		return -EINVAL;

	if (!(spc = kmem_alloc(sizeof(*spc))))
		return -ENOMEM;

	spc->min_id     = min_id;
	spc->max_id     = max_id;
	spc->size       = max_id - min_id + 1;
	spc->ids_in_use = 0;
	spc->offset     = 0;

	if (!(spc->bitmap = kmem_get_pages(calc_order(spc)))) {
		kmem_free(spc);
		return -ENOMEM;
	}

	*idspace = spc;

	return 0;
}

int
idspace_destroy(idspace_t idspace)
{
	struct idspace *spc = idspace;

	if (!spc)
		return -EINVAL;

	kmem_free_pages(spc->bitmap, calc_order(spc));
	kmem_free(spc);

	return 0;
}

int
idspace_alloc_id(idspace_t idspace, id_t request, id_t *id)
{
	struct idspace *spc = idspace;
	unsigned int bit;

	if (!spc)
		return -EINVAL;

	if ((request != ANY_ID) &&
	    ((request < spc->min_id) || (request > spc->max_id)))
		return -EINVAL;

	if (spc->size == spc->ids_in_use)
		return -ENOENT;

	if (request == ANY_ID) {
		/* Allocate any available id */
		bit = find_next_zero_bit(spc->bitmap, spc->size, spc->offset);
		/* Handle wrap-around */
		if (bit == spc->size)
			bit = find_next_zero_bit(spc->bitmap, spc->offset, 0);
		/* Next time start looking at the next id */
		spc->offset = bit + 1;
		if (spc->offset == spc->size)
			spc->offset = 0;
	} else {
		/* Allocate a specific ID */
		bit = request - spc->min_id;
	}

	if (test_and_set_bit(bit, spc->bitmap))
		return -EBUSY;

	++spc->ids_in_use;
	if (id)
		*id = bit + spc->min_id;

	return 0;
}

int
idspace_free_id(idspace_t idspace, id_t id)
{
	struct idspace *spc = idspace;
	unsigned int bit;

	if (!spc)
		return -EINVAL;

	if ((id == ANY_ID) || (id < spc->min_id) || (id > spc->max_id))
		return -EINVAL;

	bit = id - spc->min_id;
	if (test_and_clear_bit(bit, spc->bitmap) == 0)
		return -ENOENT;

	--spc->ids_in_use;

	return 0;
}

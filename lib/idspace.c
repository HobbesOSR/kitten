/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/idspace.h>
#include <lwk/spinlock.h>
#include <lwk/log2.h>
#include <arch/page.h>

struct idspace {
	spinlock_t	lock;
	id_t		min_id;
	id_t		max_id;
	size_t		size;
	size_t		ids_in_use;
	size_t		offset;
	void *		bitmap;
};

static size_t
calc_order(struct idspace *idspace)
{
	size_t pages = DIV_ROUND_UP(idspace->size, PAGE_SIZE * 8);
	return roundup_pow_of_two(pages);
}

struct idspace *
idspace_create(
	id_t			min_id,
	id_t 			max_id
)
{
	struct idspace *spc;

	if ((min_id == ANY_ID) || (max_id == ANY_ID))
		return NULL;

	if (min_id > max_id)
		return NULL;

	if (!(spc = kmem_alloc(sizeof(*spc))))
		return NULL;

	spin_lock_init(&spc->lock);

	spc->min_id     = min_id;
	spc->max_id     = max_id;
	spc->size       = max_id - min_id + 1;
	spc->ids_in_use = 0;
	spc->offset     = 0;

	if (!(spc->bitmap = kmem_get_pages(calc_order(spc)))) {
		kmem_free(spc);
		return NULL;
	}

	return spc;
}

void
idspace_destroy(
	struct idspace *	spc
)
{
	kmem_free_pages(spc->bitmap, calc_order(spc));
	kmem_free(spc);
}

id_t
idspace_alloc_id(
	struct idspace *	spc,
	id_t 			request
)
{
	unsigned int bit;
	id_t id = ERROR_ID;

	spin_lock(&spc->lock);

	if (spc->size == spc->ids_in_use)
		goto out;

	if (request != ANY_ID) {
		/* Allocate a specific ID */
		if( request < spc->min_id
		||  request > spc->max_id )
			goto out;

 		bit = request - spc->min_id;
	} else {
		/* Allocate any available id */
		bit = find_next_zero_bit(spc->bitmap, spc->size, spc->offset);
		/* Handle wrap-around */
		if (bit == spc->size)
			bit = find_next_zero_bit(spc->bitmap, spc->offset, 0);
		/* Next time start looking at the next id */
		spc->offset = bit + 1;
		if (spc->offset == spc->size)
			spc->offset = 0;
	}

	if (test_and_set_bit(bit, spc->bitmap))
		goto out;

	++spc->ids_in_use;
	id = bit + spc->min_id;
out:
	spin_unlock(&spc->lock);
	return id;
}

int
idspace_free_id(
	struct idspace *	spc,
	id_t			id
)
{
	unsigned int bit;
	int status = -EINVAL;

	spin_lock(&spc->lock);

	if ((id == ANY_ID) || (id < spc->min_id) || (id > spc->max_id))
		goto out;

	bit = id - spc->min_id;
	if (test_and_clear_bit(bit, spc->bitmap) == 0)
		goto out;

	--spc->ids_in_use;
	status = 0;
out:
	spin_unlock(&spc->lock);
	return status;
}

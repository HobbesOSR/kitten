/*
 *	linux/kernel/resource.c
 *
 * Copyright (C) 1999	Linus Torvalds
 * Copyright (C) 1999	Martin Mares <mj@ucw.cz>
 *
 * Arbitrary resource management.
 */

#include <lwk/errno.h>
#include <lwk/resource.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <arch/io.h>

struct resource ioport_resource = {
	.name	= "PCI IO",
	.start	= 0x0000,
	.end	= IO_SPACE_LIMIT,
	.flags	= IORESOURCE_IO,
};

struct resource iomem_resource = {
	.name	= "PCI mem",
	.start	= 0UL,
	.end	= ~0UL,
	.flags	= IORESOURCE_MEM,
};

static DEFINE_RWLOCK(resource_lock);

/* Return the conflict entry if you can't request it */
static struct resource * __request_resource(struct resource *root, struct resource *new)
{
	unsigned long start = new->start;
	unsigned long end = new->end;
	struct resource *tmp, **p;

	if (end < start)
		return root;
	if (start < root->start)
		return root;
	if (end > root->end)
		return root;
	p = &root->child;
	for (;;) {
		tmp = *p;
		if (!tmp || tmp->start > end) {
			new->sibling = tmp;
			*p = new;
			new->parent = root;
			return NULL;
		}
		p = &tmp->sibling;
		if (tmp->end < start)
			continue;
		return tmp;
	}
}

static int __release_resource(struct resource *old)
{
	struct resource *tmp, **p;

	BUG_ON(old->child);

	p = &old->parent->child;
	for (;;) {
		tmp = *p;
		if (!tmp)
			break;
		if (tmp == old) {
			*p = tmp->sibling;
			old->parent = NULL;
			return 0;
		}
		p = &tmp->sibling;
	}
	return -EINVAL;
}

int request_resource(struct resource *root, struct resource *new)
{
	struct resource *conflict;

	write_lock(&resource_lock);
	conflict = __request_resource(root, new);
	write_unlock(&resource_lock);
	return conflict ? -EBUSY : 0;
}

struct resource *____request_resource(struct resource *root, struct resource *new)
{
	struct resource *conflict;

	write_lock(&resource_lock);
	conflict = __request_resource(root, new);
	write_unlock(&resource_lock);
	return conflict;
}

int release_resource(struct resource *old)
{
	int retval;

	write_lock(&resource_lock);
	retval = __release_resource(old);
	write_unlock(&resource_lock);
	return retval;
}

/*
 * Find empty slot in the resource tree given range and alignment.
 */
static int find_resource(struct resource *root, struct resource *new,
			 unsigned long size,
			 unsigned long min, unsigned long max,
			 unsigned long align,
			 void (*alignf)(void *, struct resource *,
					unsigned long, unsigned long),
			 void *alignf_data)
{
	struct resource *this = root->child;

	new->start = root->start;
	/*
	 * Skip past an allocated resource that starts at 0, since the assignment
	 * of this->start - 1 to new->end below would cause an underflow.
	 */
	if (this && this->start == 0) {
		new->start = this->end + 1;
		this = this->sibling;
	}
	for(;;) {
		if (this)
			new->end = this->start - 1;
		else
			new->end = root->end;
		if (new->start < min)
			new->start = min;
		if (new->end > max)
			new->end = max;
		new->start = ALIGN(new->start, align);
		if (alignf)
			alignf(alignf_data, new, size, align);
		if (new->start < new->end && new->end - new->start >= size - 1) {
			new->end = new->start + size - 1;
			return 0;
		}
		if (!this)
			break;
		new->start = this->end + 1;
		this = this->sibling;
	}
	return -EBUSY;
}

/*
 * Allocate empty slot in the resource tree given range and alignment.
 */
int allocate_resource(struct resource *root, struct resource *new,
		      unsigned long size,
		      unsigned long min, unsigned long max,
		      unsigned long align,
		      void (*alignf)(void *, struct resource *,
				     unsigned long, unsigned long),
		      void *alignf_data)
{
	int err;

	write_lock(&resource_lock);
	err = find_resource(root, new, size, min, max, align, alignf, alignf_data);
	if (err >= 0 && __request_resource(root, new))
		err = -EBUSY;
	write_unlock(&resource_lock);
	return err;
}

/**
 * insert_resource - Inserts a resource in the resource tree
 * @parent: parent of the new resource
 * @new: new resource to insert
 *
 * Returns 0 on success, -EBUSY if the resource can't be inserted.
 *
 * This function is equivalent to request_resource when no conflict
 * happens. If a conflict happens, and the conflicting resources
 * entirely fit within the range of the new resource, then the new
 * resource is inserted and the conflicting resources become children of
 * the new resource.
 */
int insert_resource(struct resource *parent, struct resource *new)
{
	int result;
	struct resource *first, *next;

	write_lock(&resource_lock);

	for (;; parent = first) {
	 	result = 0;
		first = __request_resource(parent, new);
		if (!first)
			goto out;

		result = -EBUSY;
		if (first == parent)
			goto out;

		if ((first->start > new->start) || (first->end < new->end))
			break;
		if ((first->start == new->start) && (first->end == new->end))
			break;
	}

	for (next = first; ; next = next->sibling) {
		/* Partial overlap? Bad, and unfixable */
		if (next->start < new->start || next->end > new->end)
			goto out;
		if (!next->sibling)
			break;
		if (next->sibling->start > new->end)
			break;
	}

	result = 0;

	new->parent = parent;
	new->sibling = next->sibling;
	new->child = first;

	next->sibling = NULL;
	for (next = first; next; next = next->sibling)
		next->parent = new;

	if (parent->child == first) {
		parent->child = new;
	} else {
		next = parent->child;
		while (next->sibling != first)
			next = next->sibling;
		next->sibling = new;
	}

 out:
	write_unlock(&resource_lock);
	return result;
}

/*
 * Given an existing resource, change its start and size to match the
 * arguments.  Returns -EBUSY if it can't fit.  Existing children of
 * the resource are assumed to be immutable.
 */
int adjust_resource(struct resource *res, unsigned long start, unsigned long size)
{
	struct resource *tmp, *parent = res->parent;
	unsigned long end = start + size - 1;
	int result = -EBUSY;

	write_lock(&resource_lock);

	if ((start < parent->start) || (end > parent->end))
		goto out;

	for (tmp = res->child; tmp; tmp = tmp->sibling) {
		if ((tmp->start < start) || (tmp->end > end))
			goto out;
	}

	if (res->sibling && (res->sibling->start <= end))
		goto out;

	tmp = parent->child;
	if (tmp != res) {
		while (tmp->sibling != res)
			tmp = tmp->sibling;
		if (start <= tmp->end)
			goto out;
	}

	res->start = start;
	res->end = end;
	result = 0;

 out:
	write_unlock(&resource_lock);
	return result;
}

/**
 * resource_alignment - calculate resource's alignment
 * @res: resource pointer
 *
 * Returns alignment on success, 0 (invalid alignment) on failure.
 */
unsigned long resource_alignment(struct resource *res)
{
	switch (res->flags & (IORESOURCE_SIZEALIGN | IORESOURCE_STARTALIGN)) {
	case IORESOURCE_SIZEALIGN:
		return resource_size(res);
	case IORESOURCE_STARTALIGN:
		return res->start;
	default:
		return 0;
	}
}

/**
 * __request_region - create a new busy resource region
 * @parent: parent resource descriptor
 * @start: resource start address
 * @n: resource region size
 * @name: reserving caller's ID string
 */
struct resource * __request_region(struct resource *parent,
				   unsigned long start, unsigned long n,
				   const char *name)
{
	struct resource *res = kmem_alloc(sizeof(*res));

	if (res) {
		res->name = name;
		res->start = start;
		res->end = start + n - 1;
		res->flags = IORESOURCE_BUSY;

		write_lock(&resource_lock);

		for (;;) {
			struct resource *conflict;

			conflict = __request_resource(parent, res);
			if (!conflict)
				break;
			if (conflict != parent) {
				parent = conflict;
				if (!(conflict->flags & IORESOURCE_BUSY))
					continue;
			}

			/* Uhhuh, that didn't work out.. */
			kmem_free(res);
			res = NULL;
			break;
		}
		write_unlock(&resource_lock);
	}
	return res;
}

/**
 * __check_region - check if a resource region is busy or free
 * @parent: parent resource descriptor
 * @start: resource start address
 * @n: resource region size
 *
 * Returns 0 if the region is free at the moment it is checked,
 * returns %-EBUSY if the region is busy.
 *
 * NOTE:
 * This function is deprecated because its use is racy.
 * Even if it returns 0, a subsequent call to request_region()
 * may fail because another driver etc. just allocated the region.
 * Do NOT use it.  It will be removed from the kernel.
 */
int __check_region(struct resource *parent, unsigned long start,
			unsigned long n)
{
	struct resource * res;

	res = __request_region(parent, start, n, "check-region");
	if (!res)
		return -EBUSY;

	release_resource(res);
	kmem_free(res);
	return 0;
}

/**
 * __release_region - release a previously reserved resource region
 * @parent: parent resource descriptor
 * @start: resource start address
 * @n: resource region size
 *
 * The described resource region must match a currently busy region.
 */
void __release_region(struct resource *parent, unsigned long start,
			unsigned long n)
{
	struct resource **p;
	unsigned long end;

	p = &parent->child;
	end = start + n - 1;

	write_lock(&resource_lock);

	for (;;) {
		struct resource *res = *p;

		if (!res)
			break;
		if (res->start <= start && res->end >= end) {
			if (!(res->flags & IORESOURCE_BUSY)) {
				p = &res->child;
				continue;
			}
			if (res->start != start || res->end != end)
				break;
			*p = res->sibling;
			write_unlock(&resource_lock);
			kmem_free(res);
			return;
		}
		p = &res->sibling;
	}

	write_unlock(&resource_lock);

	printk(KERN_WARNING "Trying to free nonexistent resource "
		"<%016llx-%016llx>\n", (unsigned long long)start,
		(unsigned long long)end);
}

/* LWK specific implementation of the kmem_cache API */

#include <lwk/kernel.h>
#include <linux/slab.h>

#undef _KDBG
#define _KDBG(fmt,args...)

struct kmem_cache *
kmem_cache_create(const char *name, size_t size, size_t align,
                  unsigned long flags, void (*ctor)(void *))
{
	struct kmem_cache *cachep = kmem_alloc(sizeof(struct kmem_cache));
	if (!cachep)
		return NULL;

	cachep->size  = size;
	cachep->align = align;
	cachep->flags = flags;
	cachep->ctor  = ctor;
	strlcpy(cachep->name, name, sizeof(cachep->name));

	return cachep;
}

void
kmem_cache_destroy(struct kmem_cache *cachep)
{
	kmem_free(cachep);
}

void *
kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	size_t size = cachep->size;

	if (cachep->flags & SLAB_HWCACHE_ALIGN)
		size = max(cachep->size, (size_t)cache_line_size());

	void *objp = kmem_alloc(size);
	if (!objp) {
		if (cachep->flags & SLAB_PANIC)
			panic("kmem_cache_alloc() failed.");
		else
			return NULL;
	}

	if (cachep->ctor)
		cachep->ctor(objp);

	return objp;
}

void
kmem_cache_free(struct kmem_cache *cachep, void *objp)
{
	kmem_free(objp);
}

void *
kzalloc(size_t size, gfp_t flags)
{
	void *mem = kmem_alloc(size);
	if (!mem && (flags & GFP_ATOMIC))
		panic("Out of memory! kmalloc(GFP_ATOMIC) failed.");
	if ( current->id >= 100 )
		_KDBG("mem=%p size=%lu\n",mem,size);
	return mem;
}

void *
kmalloc(size_t size, gfp_t flags)
{
	return kzalloc(size, flags);
}

void *
kcalloc(size_t n, size_t size, gfp_t flags)
{
	if ((n != 0) && (size > ULONG_MAX / n))
		return NULL;
	return kzalloc(n * size, flags);
}

void
kfree(const void *mem)
{
	if ( current->id >= 100 )
		_KDBG("mem=%p\n",mem);
	if (mem)
		kmem_free(mem);
}

/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUX_SLAB_H_
#define	_LINUX_SLAB_H_



#include <linux/types.h>
#include <linux/gfp.h>
#include <lwk/kernel.h>
#include <lwk/cpuinfo.h>

#define ZERO_SIZE_PTR ((void *)16)

static inline void *
kzalloc(size_t size, gfp_t flags)
{
	void *mem = kmem_alloc(size);
	if (!mem && (flags & GFP_ATOMIC))
		panic("Out of memory! kmalloc(GFP_ATOMIC) failed.");
	if ( current->id >= 100 )
		_KDBG("mem=%p size=%lu\n",mem,size);
	return mem;
}

static inline void *
kmalloc(size_t size, gfp_t flags)
{
	return kzalloc(size, flags);
}


static inline void
kfree(const void *mem)
{
	if ( current->id >= 100 )
		_KDBG("mem=%p\n",mem);
	if (mem)
		kmem_free(mem);
}

static inline void *
kcalloc(size_t n, size_t size, gfp_t flags)
{
	if ((n != 0) && (size > ULONG_MAX / n))
		return NULL;
	return kzalloc(n * size, flags);
}



static inline void *
__krealloc(const void *p, size_t new_size, gfp_t flags)
{
	void *ret;
	size_t ks = 0;
    
	if (unlikely(!new_size))
		return ZERO_SIZE_PTR;
	
	if (p)
		ks = 0;//ksize(p);
	
	if (ks >= new_size)
		return (void *)p;
	
	ret = kmalloc(new_size, flags);
	if (ret && p)
		memcpy(ret, p, ks);
	
	return ret;
}
	
	
static inline void *
krealloc(const void *p, size_t new_size, gfp_t flags)
{
	void *ret;
	
	if (unlikely(!new_size)) {
		kfree(p);
		return ZERO_SIZE_PTR;
	}
	
	ret = __krealloc(p, new_size, flags);
	if (ret && p != ret)
		kfree(p);
	
	return ret;
}



#define bzero(ptr, size)       memset(ptr, 0, size)



/**
 * kmem_cache structure, holds state needed for kmem_cache_alloc().
 */
struct kmem_cache {
	size_t size;            /* size of objects allocated from the cache */
	size_t align;           /* alignment of objects allocated from the cache */
	size_t flags;           /* flags to use when allocating objects from the cache */
	void (*ctor)(void *);   /* new object constructor */
	char name[16];          /* name of the cache, useful for debugging */
};

#define SLAB_HWCACHE_ALIGN      0x00002000UL    /* Align objs on cache lines */
#define SLAB_CACHE_DMA          0x00004000UL    /* Use GFP_DMA memory */
#define SLAB_PANIC              0x00040000UL    /* Panic if kmem_cache_create() fails */
	






static inline struct kmem_cache *
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


static inline void *
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

static inline void
kmem_cache_free(struct kmem_cache *cachep, void *objp)
{
	kmem_free(objp);
}


static inline void
kmem_cache_destroy(struct kmem_cache *cachep)
{
	kmem_free(cachep);
}



#endif	/* _LINUX_SLAB_H_ */

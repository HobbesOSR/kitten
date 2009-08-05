/**
 * \file
 * LWK-specific slab allocator API header.
 * The API is the same as on Linux but the implementation is different.
 */

/*
 * Written by Mark Hemment, 1996 (markhe@nextd.demon.co.uk).
 *
 * (C) SGI 2006, Christoph Lameter
 * 	Cleaned up and restructured to ease the addition of alternative
 * 	implementations of SLAB allocators.
 */

#ifndef __LINUX_SLAB_H
#define	__LINUX_SLAB_H

#include <linux/gfp.h>
#include <linux/types.h>

/**
 * Flags to pass to kmem_cache_create().
 */
#define SLAB_HWCACHE_ALIGN	0x00002000UL	/* Align objs on cache lines */
#define SLAB_CACHE_DMA		0x00004000UL	/* Use GFP_DMA memory */
#define SLAB_PANIC		0x00040000UL	/* Panic if kmem_cache_create() fails */

/**
 * kmem_cache_create - Create a cache.
 * @name: A string which is used in /proc/slabinfo to identify this cache.
 * @size: The size of objects to be created in this cache.
 * @align: The required alignment for the objects.
 * @flags: SLAB flags
 * @ctor: A constructor for the objects.
 *
 * Returns a ptr to the cache on success, NULL on failure.
 * Cannot be called within a int, but can be interrupted.
 * The @ctor is run when new pages are allocated by the cache.
 */
extern struct kmem_cache *
kmem_cache_create(const char *name, size_t size, size_t align,
                  unsigned long flags, void (*ctor)(void *));

/**
 * kmem_cache_destroy - delete a cache
 * @cachep: the cache to destroy
 *
 * Remove a &struct kmem_cache object from the slab cache.
 *
 * It is expected this function will be called by a module when it is
 * unloaded.  This will remove the cache completely, and avoid a duplicate
 * cache being allocated each time a module is loaded and unloaded, if the
 * module doesn't have persistent in-kernel storage across loads and unloads.
 *
 * The cache must be empty before calling this function.
 *
 * The caller must guarantee that noone will allocate memory from the cache
 * during the kmem_cache_destroy().
 */
extern void
kmem_cache_destroy(struct kmem_cache *cachep);

/**
 * kmem_cache_alloc - Allocate an object
 * @cachep: The cache to allocate from.
 * @flags: the type of memory to allocate.
 *
 * Allocate an object from this cache.  The flags are only relevant
 * if the cache has no available objects.
 */
extern void *
kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags);

/**
 * kmem_cache_free - Deallocate an object
 * @cachep: The cache the allocation was from.
 * @objp: The previously allocated object.
 *
 * Free an object which was previously allocated from this
 * cache.
 */
extern void
kmem_cache_free(struct kmem_cache *cachep, void *objp);

/**
 * kmalloc - allocate some memory. The memory is not zeroed.
 * @size: how many bytes of memory are required.
 * @flags: the type of memory to allocate.
 */
extern void *
kmalloc(size_t size, gfp_t flags);

/**
 * kzalloc - allocate memory. The memory is set to zero.
 * @size: how many bytes of memory are required.
 * @flags: the type of memory to allocate.
 */
extern void *
kzalloc(size_t size, gfp_t flags);

/**
 * kcalloc - allocate memory for an array. The memory is set to zero.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate.
 */
extern void *
kcalloc(size_t n, size_t size, gfp_t flags);

/**
 * kfree - frees memory previously allocated with kmalloc() and friends.
 * @mem: address of the memory to free.
 */
extern void
kfree(const void * mem);

#endif

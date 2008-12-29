/* Copyright (c) 2007, Sandia National Laboratories */

#ifndef _LWK_KMEM_H
#define _LWK_KMEM_H

extern void kmem_create_zone(unsigned long base_addr, size_t size);
extern void kmem_add_memory(unsigned long base_addr, size_t size);

extern void *kmem_alloc(size_t size);
extern void kmem_free( const void *addr);

extern void * kmem_get_pages(unsigned long order);
extern void kmem_free_pages(const void *addr, unsigned long order);

#endif

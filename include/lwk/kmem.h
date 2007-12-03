/* Copyright (c) 2007, Sandia National Laboratories */

#ifndef _LWK_KMEM_H
#define _LWK_KMEM_H

void kmem_create_zone(unsigned long base_addr, size_t size);
void kmem_add_memory(unsigned long base_addr, size_t size);

void *kmem_alloc(size_t size);
void kmem_free(void *addr);

void * kmem_get_pages(unsigned long order);
void kmem_free_pages(void *addr, unsigned long order);

#endif

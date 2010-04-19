#ifndef PCT_PMEM_H
#define PCT_PMEM_H

#include <lwk/types.h>

paddr_t _pmem_alloc( size_t size, size_t alignment, uintptr_t arg );
void _pmem_free( paddr_t paddr );
void* _pmem_vaddr( paddr_t paddr );
paddr_t _pmem_paddr( void* vaddr );

#endif

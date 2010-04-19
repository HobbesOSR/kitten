#ifndef PCT_MEM_H
#define PCT_MEM_H

#include <sys/types.h>
#include <lwk/types.h>

void* mem_alloc( size_t );
void mem_free( void* );
paddr_t mem_paddr( void* );

#endif

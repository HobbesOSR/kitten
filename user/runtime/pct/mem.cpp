
#include <pct/mem.h>
#include <pct/pmem.h>
#include <pct/debug.h>
#include <arch/page.h>

void* mem_alloc( size_t len )
{
//    Debug1("len=%#lx\n",len);
    return _pmem_vaddr( _pmem_alloc( len, PAGE_SIZE, 0 ) );
}

void mem_free( void* ptr )
{
//    Debug1("ptr=%p\n",ptr);
    _pmem_free( _pmem_paddr( ptr) );
}

paddr_t mem_paddr( void* ptr )
{
//    Debug1("ptr=%p\n",ptr);
    return _pmem_paddr( ptr );
}

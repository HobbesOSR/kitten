
#include <pct/pmem.h>
#include <pct/debug.h>
#include <map>

extern "C" {
    #include <lwk/liblwk.h>
}

struct Entry {
    paddr_t paddr;
    size_t extent;
};

typedef std::map<void*,struct pmem_region*>   MemMap;

static MemMap memMap;

static void* map( paddr_t paddr, size_t extent );
static void unmap( void* ptr, size_t extent );

paddr_t _pmem_alloc(size_t size, size_t alignment, uintptr_t arg)
{
    Debug1("size=%#lx (%lu) alignment=%lu arg=%#lx \n",size,size,alignment,arg);

    struct pmem_region* request = new struct pmem_region;

    if ( size & ( PAGE_SIZE-1 ) ) {
        size += PAGE_SIZE;
        size &= ~(PAGE_SIZE-1);
    }

    if ( pmem_alloc_umem( size, PAGE_SIZE, request ) ) {
        return -1;
    }

    if (pmem_zero(request))
        return -1;

    void* vaddr = map( request->start, size );
    if ( vaddr == (void*) -1 ) {
        _pmem_free( request->start );
        return 0;//NULL;
    }

    memMap.insert( std::make_pair( vaddr, request ) );

//    Debug1("paddr=%#lx vaddr=%p\n", request->start, vaddr );

    return request->start;
}


void* _pmem_vaddr( paddr_t paddr )
{
    MemMap::iterator iter; 
    for ( iter = memMap.begin(); iter != memMap.end(); ++iter ) {
        if ( paddr == iter->second->start ) {
//            Debug1("paddr=%#lx vaddr=%p\n",paddr,iter->first);
            return iter->first;
        }    
    }
    return NULL;
}

paddr_t _pmem_paddr( void* vaddr )
{
    MemMap::iterator iter; 
    for ( iter = memMap.begin(); iter != memMap.end(); ++iter ) {
        if ( vaddr == iter->first ) {
//            Debug1("vaddr=%p paddr=%#lx\n",vaddr,iter->second->start);
            return iter->second->start;
        }    
    }
    return -1;
}

void _pmem_free( paddr_t paddr )
{
    //Debug1("paddr=%#lx\n",paddr);

    MemMap::iterator iter; 
    for ( iter = memMap.begin(); iter != memMap.end(); ++iter ) {
        if ( paddr == iter->second->start ) {
            break;
        }    
    }
    
    if ( iter == memMap.end() ) {
        return;
    } 

    unmap( iter->first,  iter->second->end - iter->second->start );

    iter->second->allocated = false;

    // what if this fails?
    pmem_update( iter->second );

    delete iter->second;

    memMap.erase( iter );
}

static void* map( paddr_t paddr, size_t extent )
{
    id_t my_id;
    int status;
    vaddr_t vstart;

//    Debug1( "paddr=%#lx extent=%#lx\n", paddr, extent );

    if ( ( status = aspace_get_myid( &my_id ) ) ) {
        printf("ERROR: aspace_get_myid() status=%d\n", status);
        return (void*) -1;
    }

    if ( ( status = aspace_find_hole( my_id, 0, extent,
                                            PAGE_SIZE, &vstart ) ) ) {
        printf("ERROR: aspace_find_hole() status=%d\n", status);
        return (void*) -1;
    }

//    Debug1("vstart=%#lx\n",vstart);
    if ( ( status = aspace_add_region( my_id, vstart, extent,
                VM_READ | VM_WRITE | VM_USER, PAGE_SIZE, "application") ) ) {
        printf("ERROR: aspace_add_region() status=%d\n", status);
        return (void*) -1;
    }

    if ( ( status = aspace_map_pmem( my_id, paddr, vstart, extent ) ) ) {
        printf("ERROR: aspace_map_pmem() status=%d\n", status);
        aspace_del_region( my_id, vstart, extent );
        return (void*) -1;
    }

//    Debug1( "paddr=%#lx vstart=%#lx len=%#lx\n", paddr, vstart, extent );

    return (void*) vstart;
}

static void unmap( void* ptr, size_t extent )
{
    id_t my_id;
    int status;

//    Debug1( "ptr=%p extent=%#lx\n", ptr, extent );

    if ( ( status = aspace_get_myid( &my_id ) ) ) {
        printf("ERROR: aspace_get_myid() status=%d\n", status);
        return;
    }

    if ( ( status = aspace_unmap_pmem( my_id, (vaddr_t) ptr, extent ) ) ) {
        printf("ERROR: aspace_unmap_pmem() status=%d\n", status);
    }

    if ( ( status = aspace_del_region( my_id, (vaddr_t) ptr, extent) ) ) {
        printf("ERROR: aspace_del_region() status=%d\n", status);
    }
}



#ifndef srdma_mem_h
#define srdma_mem_h

#include <srdma/types.h>
#include <srdma/dat/core.h>
#include <srdma/dat/util.h>
#include <deque>

namespace srdma {
class MemRegion
{
    public:
	typedef int Id;	
	typedef unsigned long Addr;
	typedef MemRegion* Handle;	
	MemRegion( dat::core& core, void *addr, Size );
	~MemRegion();
	int popEvent( Event* );
	void pushEvent( Event* );

	DAT_RMR_TRIPLET& rmr_triplet() {
		return m_rmr_triplet;
	}

    private:
	DAT_LMR_HANDLE		m_lmr_handle;
	DAT_LMR_TRIPLET		m_lmr_triplet;
	DAT_RMR_TRIPLET 	m_rmr_triplet;
	std::deque<Event*>	m_eventQ;
	pthread_mutex_t		m_lock;
};

inline MemRegion::MemRegion( dat::core& core, void *addr, Size size )
{
        DAT_MEM_PRIV_FLAGS flags;
        DAT_REGION_DESCRIPTION region;
        DAT_RETURN ret;

	dbg(MemRegion, "addr=%p size=%lu\n", addr, size );

        region.for_va = addr;

        m_lmr_triplet.virtual_address = (DAT_VADDR) addr;
        m_lmr_triplet.segment_length = size;

        m_rmr_triplet.virtual_address = (DAT_VADDR) addr;
        m_rmr_triplet.segment_length = size;

        flags = (DAT_MEM_PRIV_FLAGS)
            (DAT_MEM_PRIV_LOCAL_WRITE_FLAG |
             DAT_MEM_PRIV_LOCAL_READ_FLAG |
             DAT_MEM_PRIV_REMOTE_READ_FLAG | DAT_MEM_PRIV_REMOTE_WRITE_FLAG);

        ret = dat_lmr_create( core.ia_handle(),
				DAT_MEM_TYPE_VIRTUAL,
				region,
				size,
				core.pz_handle(),
				flags,
				DAT_VA_TYPE_VA,
				&m_lmr_handle,
				&m_lmr_triplet.lmr_context,
				&m_rmr_triplet.rmr_context,
				NULL,
				NULL );
	if ( ret != DAT_SUCCESS ) {
		dbgFail(MemRegion,"dat_lmr_create() %s\n",
						dat::errorStr(ret).c_str());
		throw ret;
	}
	dbg(MemRegion,"rmr_context %#x\n", m_rmr_triplet.rmr_context);
	pthread_mutex_init( &m_lock, NULL );
}

inline MemRegion::~MemRegion( ) 
{
	DAT_RETURN ret;
	ret = dat_lmr_free( m_lmr_handle );
	if ( ret != DAT_SUCCESS ) {
		dbgFail(MemRegion,"dat_lmr_free() %s\n",
						dat::errorStr(ret).c_str());
		throw -1;
	}
}

inline int MemRegion::popEvent( Event* event )
{
	pthread_mutex_lock(&m_lock);
        if (m_eventQ.empty()) {
		pthread_mutex_unlock(&m_lock);
                return false;
        }
        *event = *m_eventQ.front();
        delete m_eventQ.front();
        m_eventQ.pop_front();
        event->addr = (void*) ((unsigned long) event->addr + 
			(unsigned long) m_lmr_triplet.virtual_address );

	int ret = m_eventQ.size() + 1;
	pthread_mutex_unlock(&m_lock);
        return ret;
}

inline void MemRegion::pushEvent( Event* event )
{
	pthread_mutex_lock(&m_lock);
	m_eventQ.push_back( event );
	pthread_mutex_unlock(&m_lock);
}
	

} // namespace srdma

#endif

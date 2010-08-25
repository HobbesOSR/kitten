#ifndef _rdma_memRegion_h
#define _rdma_memRegion_h

#include <util.h>
#include <deque>
#include <types.h>
#include <infiniband/verbs.h>

const int MemMapSize = 512;

namespace rdmaPlus {

class MemRegion
{
    public:
	    typedef int Id;	
	    typedef unsigned long Addr;
	    typedef MemRegion* Handle;	
	    MemRegion( struct ibv_pd* pd, void *addr, Size,
						bool wantEvents = false );
	    ~MemRegion();
        RemoteEntry& remoteEntry();
	    int popEvent( Event* );
	    void pushEvent( Event* );

    private:
	    std::deque<Event*>	m_eventQ;
	    pthread_mutex_t		m_lock;
        struct ibv_mr*      m_mr; 
        RemoteEntry         m_remoteEntry;
};

inline MemRegion::MemRegion( struct ibv_pd* pd, void *addr, Size length, 
			bool wantEvents )
{
    dbg(MemRegion,"pd=%p addr=%p length=%#lx\n", pd, addr, length );

    m_mr = ibv_reg_mr( pd, addr, length, (ibv_access_flags) 
                    (IBV_ACCESS_REMOTE_WRITE |
                    IBV_ACCESS_LOCAL_WRITE |
                    IBV_ACCESS_REMOTE_READ ));

    if ( ! m_mr ) {
        terminal(MemRegion,"ibv_reg_mr()\n");
    }
    m_remoteEntry.addr = (uint64_t) m_mr->addr;
    m_remoteEntry.length = m_mr->length;
    m_remoteEntry.key = m_mr->rkey;
    m_remoteEntry.wantEvents = wantEvents;

	pthread_mutex_init( &m_lock, NULL );
}

inline MemRegion::~MemRegion( ) 
{
    if ( ibv_dereg_mr( m_mr ) ) {
        terminal( MemRegion, "ibv_dereg_mr()\n");
    }
    while ( ! m_eventQ.empty() ) {
        delete m_eventQ.front();
        m_eventQ.pop_front();
    }	
}

inline RemoteEntry& MemRegion::remoteEntry()
{
    return m_remoteEntry;
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
			(unsigned long) m_mr->addr );

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

} // namespace rdmaPlus

#endif

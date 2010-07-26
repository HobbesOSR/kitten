
#ifndef _rdma_rdma_h
#define _rdma_rdma_h

#include <rdma/rdma_cma.h>
#include <types.h>
#include <util.h>
#include <map>
#include <vector>

#include <pthread.h>
#include <signal.h>

#include <msgOp.h>
#include <workComp.h>
#include <msg.h>
#include <memOp.h>
#include <request.h>
#include <memRegion.h>

namespace rdmaPlus {

class Rdma {

    public:
        Rdma(); 
        ~Rdma();
        int connect( ProcId& );
        int disconnect( ProcId& );
        int isend(void* addr, size_t size, ProcId& id, Tag tag, Request& req);
        int irecv(void* addr, size_t size, ProcId& id, Tag tag, Request& req);
        int recv(void*, size_t, ProcId&, Tag, Status* );
        int send(void*, size_t, ProcId&, Tag, Status* );
        int wait( Request&, long time, Status* status = NULL );
        Request* recvCancel( Key & );

        int memRegionRegister(void *, Size, MemRegion::Id );
        int memRegionUnregister( MemRegion::Id );
        int memRegionGetEvent( MemRegion::Id, Event &);
        int memRead(ProcId &, MemRegion::Id, MemRegion::Addr far,
                            void *local, Size);
        int memWrite(ProcId &, MemRegion::Id, MemRegion::Addr far,
                            void *local, Size);

        Nid nid();
        Pid pid();

    private:
        static void* thread( void* obj );
        void* thread( );
        int listen( int port );
        int eventHandler( rdma_cm_event* event );

        static void handler(int) {
            printf("XXXXXXXXXX\n");
        }

    private:

        struct ProcIdCmp {
             bool operator() (const ProcId & lh, const ProcId & rh) {
                if (lh.nid != rh.nid)
                    return (lh.nid < rh.nid);
                if (lh.pid != rh.pid)
                    return (lh.nid < rh.nid);
                return false;
            }
        };

        typedef std::map < ProcId, Connection*, ProcIdCmp > connectionMap_t;
        connectionMap_t             m_connM;
        struct rdma_event_channel*  m_eChan;
        struct rdma_cm_id*          m_listenId;
        WorkComp*                   m_workComp;

        pthread_t                   m_thread;
        pthread_mutex_t             m_mutex;

        struct ibv_pd*              m_pd;
    
        MsgOp*                      m_msgOp;
        MemOp*                      m_memOp;

        std::vector<RemoteEntry>    m_ibvRegionM;
        struct ibv_mr*              m_ibvRegionM_mr;
        ProcId                      m_procId;
};

inline Rdma::Rdma() :
    m_procId( getMyProcID() )
{
    dbg( Rdma, "enter\n" );

    int num;
    struct ibv_context** tmp = rdma_get_devices( &num );

    if ( num != 1 ) {
        terminal( Rdma, "what context should we use? found %d\n", num);
    }

    if ( ( m_pd = ibv_alloc_pd( tmp[0] ) ) == NULL ) {
        terminal( Rdma, "ibv_alloc_pd() %s\n", strerror(errno) );
    }

    m_ibvRegionM.resize(MemMapSize);

    m_ibvRegionM_mr = ibv_reg_mr( m_pd, &m_ibvRegionM[0],
                sizeof( m_ibvRegionM[0] ) * m_ibvRegionM.size(),
                        IBV_ACCESS_REMOTE_READ ); 

    if ( ! m_ibvRegionM_mr ) {
        terminal( Connection, "\n" );
    }

    m_workComp = new WorkComp( tmp[0] );
    m_memOp = new MemOp( m_pd, m_ibvRegionM );
    m_msgOp = new MsgOp( m_pd, *m_memOp );

    rdma_free_devices( tmp);

    if( ! ( m_eChan = rdma_create_event_channel() ) ) {
        terminal( Rdma, "rdma_create_event_channel()\n");
    }

    pthread_mutex_init( &m_mutex, NULL );

    if ( pthread_create( &m_thread, NULL, thread, this ) ) {
        terminal( Rdma, "pthread_create()\n");
    }

    if ( listen( (int) getpid() ) ) {
        terminal( Rdma, "listen\n");
    }

    signal( SIGUSR1, handler );
}

inline Rdma::~Rdma()
{
    dbg( Rdma, "enter\n" );

    if ( pthread_cancel( m_thread ) ) {
        terminal( Rdma, "pthread_cancel()\n");
    }

    if ( pthread_join( m_thread, NULL ) ) {
        terminal( Rdma, "pthread_join()\n");
    }

    rdma_destroy_id ( m_listenId );
    rdma_destroy_event_channel( m_eChan ); 
    delete m_msgOp;
    delete m_memOp;
    delete m_workComp;

    if ( ibv_dereg_mr( m_ibvRegionM_mr ) ) {
        terminal( Rdma, "ibv_dereg() %s\n",strerror(errno));
    }

    if ( ibv_dealloc_pd( m_pd ) ) {
        terminal( Rdma, "ibv_dealloc_pd() %s\n",strerror(errno));
    }

    dbg( Rdma, "return\n" );
}

inline int Rdma::listen( int port )
{
    dbg(Rdma,"port=%d\n",port);
    if ( rdma_create_id( m_eChan, &m_listenId, this, RDMA_PS_TCP ) ) {
        terminal( Rdma, "rdma_create_id()\n");
    }

    struct sockaddr_in addr_in;
    memset(&addr_in,0,sizeof(addr_in));

    addr_in.sin_family = PF_INET;
    addr_in.sin_port = htons( getpid() );

    if ( rdma_bind_addr( m_listenId, (struct sockaddr *) &addr_in ) ) {
        terminal( Rdma, "rdma_bind_addr()\n");
    }

    if ( rdma_listen( m_listenId, 10 ) ) { 
        terminal( Rdma, "rdma_listen()\n");
    }
    return 0;
}

inline int Rdma::connect( ProcId& procId )
{
    dbg( Rdma, "[%#x,%d]\n", procId.nid, procId.pid );

    pthread_mutex_lock(&m_mutex);
    if ( m_connM.find(procId) != m_connM.end() ) {
        pthread_mutex_unlock(&m_mutex);
        dbgFail(Rdma,"%#x,%d\n",procId.nid,procId.pid);
        return -1;
    }
    
    Connection* conn = new Connection( m_pd, m_ibvRegionM_mr,
                        m_workComp->compChan(),
                        m_msgOp->recvHandler(), sizeof(Msg), procId, m_eChan );
    m_connM[procId] = conn;
    pthread_mutex_unlock(&m_mutex);
    conn->wait4connect();
    
    return 0;
}

inline int Rdma::disconnect( ProcId& procId )
{
    dbg( Rdma, "[%#x,%d]\n", procId.nid, procId.pid );

    pthread_mutex_lock(&m_mutex);
    if ( m_connM.find( procId ) == m_connM.end() ) {
        pthread_mutex_unlock(&m_mutex);
        dbgFail(Rdma, "couldn't find id [%#x,%d]\n", procId.nid, procId.pid );
        return -1;
    }
    // fix me
    pthread_mutex_unlock(&m_mutex);

    Connection* conn = m_connM[procId];

    conn->disconnect();

    delete conn;

    m_connM.erase(procId);

    return 0;
}

inline int Rdma::eventHandler( rdma_cm_event* event )
{
    int ret = 0;

    dbg( Rdma, "`%s` status=%d\n", rdma_event_str(event->event), event->status);

    switch (event->event) {
    case RDMA_CM_EVENT_ADDR_RESOLVED:
        ret = (*(Connection*) event->id->context).addrResolved();
        break;

    case RDMA_CM_EVENT_ROUTE_RESOLVED:
        ret = (*(Connection*) event->id->context).routeResolved();
        break;

    case RDMA_CM_EVENT_CONNECT_REQUEST:
        {
            Connection *conn = new Connection( m_pd, m_ibvRegionM_mr, 
                        m_workComp->compChan(),
                        m_msgOp->recvHandler(), sizeof(Msg), 
                        event->param.conn.private_data, event->id );   
            m_connM[conn->farProcId()] = conn;
        }
        break;

    case RDMA_CM_EVENT_ESTABLISHED:
        ret = (*(Connection*) event->id->context).established( event );
        break;

    case RDMA_CM_EVENT_ADDR_ERROR:
    case RDMA_CM_EVENT_ROUTE_ERROR:
    case RDMA_CM_EVENT_CONNECT_ERROR:
    case RDMA_CM_EVENT_UNREACHABLE:
    case RDMA_CM_EVENT_REJECTED:
        terminal( Rdma, "%s status=%d\n",
                rdma_event_str(event->event), event->status);
        break;

    case RDMA_CM_EVENT_DISCONNECTED:
        {
            ProcId procId = (*(Connection*) event->id->context).farProcId();
            dbg(Rdma,"id=[%#x,%d]\n",procId.nid,procId.pid);
        
            if ( m_connM.find(procId) != m_connM.end() ) {
                m_connM[procId]->disconnected();
                if ( ! m_connM[procId]->disconnecting()  ) {
                    m_connM.erase(procId);
                    delete (Connection*) event->id->context;
                }
            } else {
                dbgFail( Rdma, "couldn't find connection\n");
            }
        }
        break;

    case RDMA_CM_EVENT_DEVICE_REMOVAL:
        terminal( Rdma, "\n");
        break;
    default:
        terminal( Rdma, "%s status=%d\n",
                rdma_event_str(event->event), event->status);
        break;
    }
    return ret;

    return 0;
}

inline void* Rdma::thread( void* obj ) {
    return ( (Rdma*) obj)->thread();  
}

inline void* Rdma::thread()
{
    dbg( Rdma, "enter\n" );
    while(1) {
        struct rdma_cm_event* tmp;

	//printf("calling rdma_get_cm_event()\n");
        if ( rdma_get_cm_event( m_eChan, &tmp ) ) {
            dbgFail( Rdma,"rdma_get_cm_event()\n");
            return NULL;
        }

        // we want to call rdma_destroy_id() from within the eventHandler
        // however we can't until the event is acked, given this is a slow
        // path we are going to create a copy of the event
        struct rdma_cm_event event;

        memcpy( &event, tmp, sizeof(event) );

        if ( tmp->param.conn.private_data ) {
            event.param.conn.private_data = 
                    malloc( tmp->param.conn.private_data_len );

            memcpy( (void*)event.param.conn.private_data,
                    tmp->param.conn.private_data,
                    tmp->param.conn.private_data_len );
        }

        if ( rdma_ack_cm_event( tmp ) ) {
            terminal( Rdma,"rdma_get_cm_event()\n");
        }

        if ( eventHandler( &event ) ) {
            terminal( Rdma,"eventHandler()\n");
        }

        if ( event.param.conn.private_data ) {
            free( (void*) event.param.conn.private_data );
        }
    }
    return NULL;
}

inline int Rdma::isend(void* addr, size_t length, ProcId& id,
                                            Tag tag, Request& req)
{
    if ( m_connM.find( id ) == m_connM.end() ) {
        dbgFail(Rdma,"not connected [%#x,%d]\n", id.nid, id.pid );
        return -1;
    }
    if ( ! m_connM[id]->connected() ) {
        dbgFail(Rdma,"not connected [%#x,%d]\n", id.nid, id.pid );
        return -1;
    }

    return m_msgOp->isend( *m_connM[id], addr, length, id, tag, req );
}

inline int Rdma::irecv(void* addr, size_t length, ProcId& id,
                                            Tag tag, Request& req)
{
    return m_msgOp->irecv( addr, length, id, tag, req );
}

inline int Rdma::recv(void* addr, size_t size, ProcId& id,
                                            Tag tag, Status* status )
{
    Request req;
    int ret;

    if ( ( ret = irecv( addr, size, id, tag, req ) ) ) {
        return ret;
    } 
    return req.wait( -1, status );
}
    
inline int Rdma::send(void* addr, size_t size, ProcId& id,
                                            Tag tag, Status* status )
{
    Request req;
    int ret;

    if ( ( ret = isend( addr, size, id, tag, req ) ) ) {
        return ret;
    } 
    return req.wait( -1, status );
}

inline int Rdma::wait( Request& req, long time, Status* status )
{
    return req.wait( time, status );
}

inline Request* Rdma::recvCancel(Key & key)
{
        return m_msgOp->recv_cancel( key );
}

inline int Rdma::memRegionRegister(void *addr, Size size, MemRegion::Id id )
{   
    return m_memOp->registerRegion( addr, size, id );
}

inline int Rdma::memRegionUnregister( MemRegion::Id id )
{   
    return m_memOp->unregisterRegion( id );
}

inline int Rdma::memRegionGetEvent( MemRegion::Id id, Event& event )
{   
    return m_memOp->getRegionEvent( id, event );
}

inline int Rdma::memRead(ProcId & id, MemRegion::Id region,
                        MemRegion::Addr addr, void *local, Size length )
{       
    if ( m_connM.find( id ) == m_connM.end() ) {
        dbgFail(Rdma,"not connected [%#x,%d]\n", id.nid, id.pid );
        return -1;
    }
    if ( ! m_connM[id]->connected() ) {
        dbgFail(Rdma,"not connected [%#x,%d]\n", id.nid, id.pid );
        return -1;
    }

    return m_memOp->memOp( *m_connM[id], Read, region, addr, local, length );
}

inline int Rdma::memWrite(ProcId & id, MemRegion::Id region,
                        MemRegion::Addr addr, void *local, Size length)
{       
    if ( m_connM.find( id ) == m_connM.end() ) {
        dbgFail(Rdma,"not connected [%#x,%d]\n", id.nid, id.pid );
        return -1;
    }
    if ( ! m_connM[id]->connected() ) {
        dbgFail(Rdma,"not connected [%#x,%d]\n", id.nid, id.pid );
        return -1;
    }

    return m_memOp->memOp( *m_connM[id], Write, region, addr, local, length );
}

inline Pid Rdma::pid()
{                           
        return m_procId.pid;
}       
        
inline Nid Rdma::nid()
{
        return m_procId.nid;
}

} // namespace rdmaPlus

#endif

#ifndef _rdma_memOp_h
#define _rdma_memOp_h

#include <memRegion.h>
#include <connection.h>
#include <errno.h>

namespace rdmaPlus {

class MemOp {

    public:
        MemOp( struct ibv_pd*, std::vector<RemoteEntry>& );
        int registerRegion( void*, Size, MemRegion::Id );
        int unregisterRegion( MemRegion::Id );
        int getRegionEvent( MemRegion::Id, Event& );
        int memOp( Connection&, memOp_t op, MemRegion::Id region,
            MemRegion::Addr addr, void *local, Size length);
        bool memEvent( MemRegion::Id, memOp_t, MemRegion::Addr, Size);

    private:
        bool wakeUpHandler( struct ibv_wc&, sem_t* );
        bool cleanUpHandler( struct ibv_wc&, MsgObj* );

    private:
        typedef EventHandler1Arg< MemOp, bool, struct ibv_wc&,
                                sem_t* > wakeUpHandler_t;        
        typedef EventHandler1Arg< MemOp, bool, struct ibv_wc&,
                                MsgObj* > cleanUpHandler_t;        

        struct ibv_pd*  m_pd;

        std::vector<RemoteEntry>&    m_ibvRegionM;

        std::map < MemRegion::Id, MemRegion* >  m_regionMap;
};

inline MemOp::MemOp( struct ibv_pd* pd, std::vector<RemoteEntry>& map  ) :
    m_pd( pd ), 
    m_ibvRegionM( map )
{
}

inline int MemOp::registerRegion( void*addr, Size length, MemRegion::Id id )
{
    dbg(MemOp, "addr=%p length=%lu id=%#x\n", addr, length, id);
    if ( id >= MemMapSize ) {
        dbgFail(MemOp,"invalid Id %d\n", id );
        return -1;
    } 

    if ( m_regionMap.find( id ) != m_regionMap.end() ) {
        dbgFail(memOp, "invalid Id %d\n", id );
        return -1;
    }

    m_regionMap[id] = new MemRegion( m_pd, addr, length );
    m_ibvRegionM[id] = m_regionMap[id]->remoteEntry();;
    return 0;
}

inline int MemOp::unregisterRegion( MemRegion::Id id )
{
    dbg(MemOp, "id=%#x\n", id );
    if ( m_regionMap.find( id ) == m_regionMap.end() ) {
        dbgFail(MemOp, "invalid Id %d\n", id );
        return -1;
    }
    delete m_regionMap[ id ];
    m_regionMap.erase( id );

    m_ibvRegionM[id].key = 0;
    return 0;
}

inline int MemOp::getRegionEvent( MemRegion::Id id, Event& event )
{
    if ( m_regionMap.find( id ) == m_regionMap.end() ) {
        dbgFail(MemOp, "invalid id %d\n", id );
        return -1;
    }
    return m_regionMap[id]->popEvent( &event );
}

inline bool MemOp::memEvent( MemRegion::Id id, memOp_t op,
                         MemRegion::Addr addr, Size length)
{
    dbg(MemOp, "op=%s raddr=%#x:%#lx length=%lu\n",
            op == Write ? "Write" : "read", id, addr, length);

    if ( m_regionMap.find( id ) == m_regionMap.end() ) {
        dbgFail(MemOp, "invalid id %d\n", id );
        return -1;
    }

    Event *event = new Event;
    event->op = op;
    event->addr = (void*) addr;
    event->length = length;
    
    m_regionMap[id]->pushEvent( event );
    
    return 0;
}

inline int MemOp::memOp( Connection& conn, memOp_t op, MemRegion::Id region,
                        MemRegion::Addr addr, void *local, Size length)
{       
    dbg( MemOp, "raddr=%#x:%#lx laddr=%p length=%lu\n",
                                        region, addr, local, length );
        
    RemoteEntry* tmp = conn.remoteEntry( region );
    if ( ! tmp ) {
        dbgFail( MemOp, "couldn't find remoteEntry\n");
        return -1;
    }

    dbg( MemOp, "remote addr=%#lx length=%d key=%#x\n",
                                            tmp->addr, tmp->length, tmp->key );
        
    if ( tmp->addr + addr + length >  tmp->addr + tmp->length ) {
        dbgFail( MemOp,"invalid address + length\n");
        return -1;
    }

    struct ibv_mr* mr = ibv_reg_mr( m_pd, local, length,
                        ( ibv_access_flags )
                        (IBV_ACCESS_REMOTE_WRITE | 
                        IBV_ACCESS_REMOTE_READ |
                        IBV_ACCESS_LOCAL_WRITE ) );

    if ( ! mr ) {
        terminal( MemOp, "ibv_reg_mr()\n");
    }

    sem_t   sem;
    if ( sem_init(&sem,0,0) ) {
        terminal( MemOp,"sem_init()\n");
    }
    wakeUpHandler_t* handler = new
                    wakeUpHandler_t( this, &MemOp::wakeUpHandler, &sem );

    struct ibv_sge sge;
    sge.addr = (uint64_t) local;
    sge.length = length;
    sge.lkey = mr->lkey;

    if ( op == Write ) {
        conn.write( tmp->key, tmp->addr + addr, &sge, handler );
    } else {
        conn.read( tmp->key, tmp->addr + addr, &sge, handler );
    }

    dbg( MemOp, "wait\n");
    while ( sem_wait(&sem) ) {
        if ( errno == EINTR ) {
            dbg( MemOp,"EINTR\n");
        } else {
            throw -1;
        }
    }

    dbg( MemOp, "go\n");
    sem_destroy(&sem);

    if ( ibv_dereg_mr( mr ) ) {
        terminal( MemOp, "ibv_dereg_mr()\n" );
    }

    if ( tmp->wantEvents ) {
        MsgObj* msgObj = new MsgObj( m_pd, MsgHdr::RdmaResp );

        msgObj->msg().hdr.u.rdmaResp.id = region;
        msgObj->msg().hdr.u.rdmaResp.op = op;
        msgObj->msg().hdr.u.rdmaResp.addr = addr;
        msgObj->msg().hdr.u.rdmaResp.length = length;

        struct ibv_sge msg_sge;
        msg_sge.addr = (uintptr_t) &msgObj->msg();
        msg_sge.length = sizeof msgObj->msg();
        msg_sge.lkey = msgObj->lkey(); 

        conn.send( &msg_sge, 
            new cleanUpHandler_t( this, &MemOp::cleanUpHandler, msgObj ) );
    }
    return 0;
}

inline bool MemOp::wakeUpHandler( struct ibv_wc& wc, sem_t* sem )
{
    dbg(MemOp,"\n");
    if ( wc.status != IBV_WC_SUCCESS ) {
        dbgFail(MemOp,"wc.status=%#x\n",wc.status);
        return true;
    }

    sem_post(sem);
    return true;
}

inline bool MemOp::cleanUpHandler( struct ibv_wc& wc, MsgObj* msgObj )
{
    dbg(MemOp,"delete MsgObj\n");
    if ( wc.status != IBV_WC_SUCCESS ) {
        dbgFail(MemOp,"\n");
        return true;
    }

    delete msgObj;
    return true;
}

} // namespace rdmaPlus

#endif

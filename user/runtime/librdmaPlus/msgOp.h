#ifndef _rdma_msgOp_h
#define _rdma_msgOp_h

#include <request.h>
#include <memOp.h>

namespace rdmaPlus {

class MsgOp {

        typedef EventHandler2< MsgOp, bool, void *, Connection& > recvHandler_t;

    public:
        MsgOp( struct ibv_pd*, MemOp& );
        ~MsgOp( );
        int irecv( void*, size_t, ProcId&, Tag, Request&);
        int isend( Connection&, void*, size_t, ProcId&, Tag, Request&);

        Request* recv_cancel(Key& key);

        recvHandler_t& recvHandler() { return m_recvHandler; }

    private:          // types
        struct KeyCmp {
            bool operator() (const Key & lh, const Key & rh) {
                if (lh.count != rh.count)
                    return (lh.count < rh.count);
                if (lh.tag != rh.tag)
                    return (lh.tag < rh.tag);
                if (lh.id.nid != rh.id.nid)
                    return (lh.tag < rh.tag);
                if (lh.id.pid != rh.id.pid)
                    return (lh.tag < rh.tag);
                return false;
            }
        };

        typedef EventHandler2Arg< MsgOp, bool, struct ibv_wc&,
                                Request&, MsgObj* > sendHandler_t;

        typedef EventHandler3Arg< MsgOp, bool, struct ibv_wc&,
                                Request&, uint64_t, Connection& > readHandler_t;

        bool sendHandler( struct ibv_wc&, Request&, MsgObj* );
        bool readHandler( struct ibv_wc&, Request&, uint64_t, Connection& );
        bool recvHandler( void*, Connection& );

        bool req( Msg&, Connection& );
        bool ack( Msg&, Connection& );
        bool rdmaResp( Msg& msg, Connection& conn );
        bool keyCmp(const Key & lh, const Key & rh);
        Request *reqFind(Key & key);
        std::pair<Msg*,Connection*> findRecvMsg( Key& );

        struct ibv_pd*              m_pd;
        MemOp&                       m_memOp;
        recvHandler_t               m_recvHandler;
        
        typedef std::multimap < Key,
                        std::pair< Msg*, Connection* >, KeyCmp > msgMap_t;
        typedef std::multimap < Key, Request *, KeyCmp > reqMap_t;
      
        msgMap_t m_msgMap;
        reqMap_t m_recvMap;

        pthread_mutex_t     m_mutex;
};

inline MsgOp::MsgOp( struct ibv_pd* pd, MemOp& memOp ) :
    m_pd ( pd ),
    m_memOp( memOp ),
    m_recvHandler( recvHandler_t( this, &MsgOp::recvHandler ) )
{
    pthread_mutex_init(&m_mutex, NULL );
}

inline MsgOp::~MsgOp()
{
}
    
inline int MsgOp::isend( Connection& conn, void* addr, size_t length,
                                ProcId& id, Tag tag, Request& req)
{
    dbg(MsgOp,"addr=%p length=%lu id=[%#x,%d] tag=%x\n",
                                    addr,length, id.nid, id.pid, tag );

    req.init( m_pd, addr, length, id, tag );

    MsgObj* msg = new MsgObj( m_pd, MsgHdr::Req, (uint64_t) &req, req.rkey(),
                                (uint64_t) req.buf(), length, tag ); 
    sendHandler_t* handler = NULL;

    if ( length <= Msg::ShortMsgLength ) {
        memcpy( msg->msg().body, addr, length) ;
        handler = new sendHandler_t( this, &MsgOp::sendHandler, req, msg );
    }

    struct ibv_sge list;
    list.addr = (uintptr_t) &msg->msg();
    list.length = sizeof msg->msg();
    list.lkey = msg->lkey();

    conn.send( &list, handler );
    return 0;
}

inline int MsgOp::irecv( void* addr, size_t length,
                                ProcId& id, Tag tag, Request& req)
{
    dbg(MsgOp,"addr=%p length=%lu id=[%#x,%d] tag=%x\n",
                                    addr,length, id.nid, id.pid, tag );

    req.init( m_pd, addr, length, id, tag );

    std::pair<Msg*,Connection*> tmp = findRecvMsg( req.key() );

    dbg(MsgOp,"msg=%p conn=%p \n",tmp.first,tmp.second);
    if ( tmp.first ) {
        if ( tmp.first->hdr.u.req.length <= Msg::ShortMsgLength ) {
            
            dbg(MsgOp,"found short\n");
            memcpy( req.buf(), tmp.first->body, tmp.first->hdr.u.req.length );
            req.wake();
        } else {

            dbg(MsgOp,"found long\n");
        
            readHandler_t* handler =
                new readHandler_t( this, &MsgOp::readHandler,
                                      req, tmp.first->hdr.cookie, *tmp.second );

            struct ibv_sge sge;
            sge.addr = (uint64_t) req.buf();
            sge.length = req.length();
            sge.lkey = req.lkey();

            tmp.second->read( tmp.first->hdr.u.req.rkey, 
                                tmp.first->hdr.u.req.raddr,
                                &sge, handler );
        }
        delete tmp.first;
    } else {
        dbg(MsgOp,"save req\n");
        pthread_mutex_lock(&m_mutex);
        m_recvMap.insert(std::make_pair( req.key(), &req));
        pthread_mutex_unlock(&m_mutex);
    }
    return 0;
}

inline bool MsgOp::sendHandler( struct ibv_wc& wc, Request& req, MsgObj* msg )
{   
    dbg( MsgOp, "enter\n");
    if ( wc.status != IBV_WC_SUCCESS ) {
        dbgFail(MsgOp,"\n");
    }

    // need to pass failure somehow
    req.wake();
    delete msg;
    return true;
}

inline bool MsgOp::readHandler( struct ibv_wc& wc, Request& req, uint64_t cookie,
                                                        Connection& conn )
{   
    dbg( MsgOp, "cookie=%#lx\n",cookie);

    if ( wc.status != IBV_WC_SUCCESS ) {
        dbgFail(MsgOp,"\n");
        // handle this
        return true;
    }

    MsgObj* msgObj = new MsgObj( m_pd, MsgHdr::Ack, cookie  );

    sendHandler_t* handler =
                new sendHandler_t( this, &MsgOp::sendHandler, req, msgObj );

    struct ibv_sge list;
    list.addr = (uintptr_t) &msgObj->msg();
    list.length = sizeof msgObj->msg();
    list.lkey = msgObj->lkey();

    conn.send( &list, handler );

    return true;
}

inline bool MsgOp::recvHandler( void* ptr, Connection& conn )
{   
    Msg& msg = *(Msg*) ptr; 
    dbg( MsgOp, "type=%d\n", msg.hdr.type);
    
    switch( msg.hdr.type ) {
        case MsgHdr::Req:
            req( msg, conn );
            break;
        case MsgHdr::Ack:
            ack( msg, conn );
            break;
        case MsgHdr::RdmaResp:
            rdmaResp( msg, conn );
            break;
    }
    return false;
}

inline bool MsgOp::req( Msg& msg, Connection& conn )
{   
    Key key;
    key.id    = conn.farProcId(); 
    key.tag   = msg.hdr.u.req.tag;
    key.count = msg.hdr.u.req.length;

    dbg(MsgOp,"id=[%#x,%d] tag=%#x count=%lu cookie=%#lx\n",
               key.id.nid,key.id.pid, key.tag, key.count, msg.hdr.cookie );
    
    Request* req = reqFind( key ); 
    if ( req ) {
        req->key() = key;
        if ( key.count <= Msg::ShortMsgLength ) {
            dbg(MsgOp,"found short\n");
            memcpy( req->buf(), msg.body, key.count );
            req->wake();
        } else {
            dbg(MsgOp,"found long\n");

            readHandler_t* handler =
                new readHandler_t( this, &MsgOp::readHandler, *req,
                                                msg.hdr.cookie, conn );

            struct ibv_sge sge;
            sge.addr = (uint64_t) req->buf();
            sge.length = req->length();
            sge.lkey = req->lkey();
            conn.read( msg.hdr.u.req.rkey, msg.hdr.u.req.raddr, &sge, handler );
        }
    } else {
        dbg(MsgOp,"save msg\n");
        pthread_mutex_lock(&m_mutex);
        std::pair<Msg*,Connection*> foo(NULL, NULL);
        foo.first = new Msg;  
        foo.second = &conn;
        memcpy( foo.first, &msg, sizeof( msg ) );
        m_msgMap.insert(
                std::make_pair( key, foo ) );
        pthread_mutex_unlock(&m_mutex);
    }
    return false;
}
inline bool MsgOp::ack( Msg& msg, Connection& conn )
{   
    dbg(MsgOp,"cookie=%#lx\n",msg.hdr.cookie);

    Request* req = (Request*) msg.hdr.cookie;

    req->wake();
    return false;
}

inline bool MsgOp::rdmaResp( Msg& msg, Connection& conn )
{
    dbg(MsgOp,"\n");

    m_memOp.memEvent( msg.hdr.u.rdmaResp.id, msg.hdr.u.rdmaResp.op,
            msg.hdr.u.rdmaResp.addr, msg.hdr.u.rdmaResp.length );

    return false;
}

inline bool MsgOp::keyCmp(const Key & lh, const Key & rh)
{
    if (lh.count > rh.count)
        return false;
    if (lh.tag != (Tag) - 1 && lh.tag != rh.tag)
        return false;
    if (lh.id.nid != (Nid) - 1 && lh.id.nid != rh.id.nid)
        return false;
    if (lh.id.pid != (Pid) - 1 && lh.id.pid != rh.id.pid)
        return false;
    return true;
}

inline Request *MsgOp::reqFind(Key & key)
{
    reqMap_t::iterator iter;
    dbg(MsgOp, "Find nid=%#x pid=%d tag=%#x size=%lu\n",
        key.id.nid, key.id.pid, key.tag, key.count);

    pthread_mutex_lock(&m_mutex);
    for (iter = m_recvMap.begin(); iter != m_recvMap.end(); ++iter) {

        dbg(MsgOp, "   nid=%#x pid=%d tag=%#x size=%lu\n",
            (*iter).first.id.nid,
            (*iter).first.id.pid,
            (*iter).first.tag, (*iter).first.count);

        if (!keyCmp((*iter).first, key))
            continue;

        Request *req = (*iter).second;

        m_recvMap.erase(iter);
        pthread_mutex_unlock(&m_mutex);
        return req;
    }
    pthread_mutex_unlock(&m_mutex);
    return NULL;
}

inline std::pair<Msg*,Connection* > MsgOp::findRecvMsg( Key & key )
{
    pthread_mutex_lock(&m_mutex);

    std::pair< Msg*, Connection* > retval;

    for (msgMap_t::iterator iter = m_msgMap.begin();
         iter != m_msgMap.end(); iter++) {
            dbg(MsgOp, "    id=[%#x,=%d] tag=%#x size=%lu\n",
                    (*iter).first.id.nid, (*iter).first.id.pid,
                    (*iter).first.tag, (*iter).first.count);

            if (!keyCmp(key, (*iter).first)) {
                continue;
            }
            retval = (*iter).second;
            m_msgMap.erase(iter);
            break;
    }
    pthread_mutex_unlock(&m_mutex);
    dbg(MsgOp,"msg=%p\n",retval.first);

    return retval;
}

inline Request* MsgOp::recv_cancel(Key& key)
{
    return reqFind( key );
}


} // namespace rdmaPlus

#endif

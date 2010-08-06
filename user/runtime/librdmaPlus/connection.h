
#ifndef _rdma_connection_h
#define _rdma_connection_h

#include <semaphore.h>
#include <rdma/rdma_cma.h>
#include <util.h>
#include <procId.h>
#include <eventHandler.h>
#include <vector>
#include <deque>
#include <string.h>
#include <errno.h>

#include <memRegion.h>

namespace rdmaPlus {

class Connection {

        typedef EventHandlerBase2< bool, void*, Connection& > recvHandler_t;
        typedef EventHandlerBase< bool, struct ibv_wc& > handler_t;

    public:

        Connection( struct ibv_pd*, struct ibv_mr*, ibv_comp_channel*,
            recvHandler_t&, int bufSize, ProcId&, struct rdma_event_channel* );
        Connection( struct ibv_pd*, struct ibv_mr*, ibv_comp_channel*,
            recvHandler_t&, int bufSize, const void*, struct rdma_cm_id* );
        ~Connection();

        int addrResolved();
        int routeResolved();
        int established( struct rdma_cm_event* event );
        int disconnect();
        void disconnected();

    
        void wait4connect();
        bool disconnecting() { return m_state == Disconnecting; }
        bool connected() { return m_state == Connected; }
        ProcId farProcId() { return m_farId; };

        int send( struct ibv_sge*, handler_t* handler = NULL );
        int read( uint32_t rkey, uint64_t addr, 
                        struct ibv_sge* sge, handler_t* handler = NULL );
        int write( uint32_t rkey, uint64_t remote_addr, 
                        struct ibv_sge* sge, handler_t* handler = NULL );

        RemoteEntry* remoteEntry( MemRegion::Id );

    private:

        struct QPInfo {
            int qpn;
            int lid;
            int psn;
        };

        struct SendQEntry {
            handler_t* handler;
            struct ibv_sge  sge;  
            ibv_wr_opcode   opcode;
            uint32_t        imm_data;
            uint32_t        rkey;
            uint64_t        remote_addr; 
        };  

        struct PrivateData {
            ProcId  procId;
            QPInfo  qpInfo;
            uint32_t rkey;
            uint64_t raddr;
        };

        typedef EventHandler1Arg< Connection, bool,
                    struct ibv_wc&, struct ibv_recv_wr*> _recvHandler_t;

        typedef EventHandler1Arg< Connection, bool, struct ibv_wc&,
                                sem_t* > memHandler_t;

        int init();
        int postRecvs();
        void initAckQP();
        void makeReadyAckQP( QPInfo&, int my_psn, int port );
        int postAckRecv();
        int postAckSend( uint32_t );
        bool recvHandler( struct ibv_wc&, struct ibv_recv_wr* );
        bool recvAckEvent( struct ibv_wc&);

        int post_wr( struct ibv_sge* list,
                            handler_t* handler = NULL, 
                            ibv_wr_opcode opcode = IBV_WR_SEND,
                            uint32_t imm_data = 0,
                            uint32_t rkey = 0,
                            uint64_t remote_addr = 0);

        int q_post_wr( struct ibv_sge* list,
                            handler_t* handler = NULL, 
                            ibv_wr_opcode opcode = IBV_WR_SEND,
                            uint32_t imm_data = 0,
                            uint32_t rkey = 0,
                            uint64_t remote_addr = 0);

        bool memHandler( struct ibv_wc&, sem_t* mutex );

    private:

        enum { NewClient, NewServer, Connected, Disconnecting } m_state;

        struct ibv_comp_channel* m_compChan;
        struct ibv_pd*      m_pd; 
        struct rdma_cm_id*  m_cm_id;
        struct ibv_cq*      m_cq;
        struct ibv_mr*      m_mr;
        std::vector<unsigned char>         m_mem;
        ProcId              m_myId;
        ProcId              m_farId;

        sem_t               m_sem;
        pthread_mutex_t     m_sendQMutex;

        struct ibv_qp*      m_ackQP;
        int                 m_psn;

        EventHandler<Connection, bool, struct ibv_wc&> m_recvAckEvent; 
        recvHandler_t&     m_recvHandler;

        int                 m_sendCredit;
        int                 m_recvCredit;
        int                 m_creditThreshold;
        unsigned int        m_bufSize;

        std::deque<SendQEntry> m_sendQ;

        std::vector< RemoteEntry >  m_remoteMap;

        uint32_t            m_rkey;
        uint64_t            m_raddr;
        struct ibv_mr*      m_mem_mr;
        struct ibv_mr*      m_foo_mr;
        const int          m_port;
        const int          m_lid;
};

inline Connection::Connection( struct ibv_pd* pd,
                                struct ibv_mr* mr,
                                struct ibv_comp_channel* compChan,
                                recvHandler_t& recvHandler,
                                int bufSize,
                                ProcId& id,
                                struct rdma_event_channel* channel ) : 
    m_state( NewClient ),
    m_compChan( compChan ),
    m_pd( pd ),
    m_myId( getMyProcID() ),
    m_farId ( id ),
    m_recvAckEvent( EventHandler< Connection, bool, struct ibv_wc& >
                                    ( this, &Connection::recvAckEvent ) ),
    m_recvHandler( recvHandler ),
    m_sendCredit( 10 ),
    m_recvCredit( 0 ),
    m_creditThreshold( m_sendCredit/2 ),
    m_bufSize( bufSize ),
    m_foo_mr( mr ),
    m_port( 1 ),
    m_lid( get_local_lid( Connection::m_port ) )
{
    dbg( Connection, "[%#x,%d]\n", id.nid, id.pid );

    srand48( getpid() * time(NULL) );
    m_psn = lrand48() & 0xffffff;

    m_remoteMap.resize(MemMapSize);

    m_mem_mr = ibv_reg_mr( m_pd, &m_remoteMap[0],
        sizeof( m_remoteMap[0] ) * m_remoteMap.size(), 
                    ( ibv_access_flags )
                    ( IBV_ACCESS_LOCAL_WRITE|
                    IBV_ACCESS_REMOTE_WRITE ) );

    if ( ! m_mem_mr ) {
        terminal( Connection, "\n" );
    }

    if ( rdma_create_id( channel, &m_cm_id, this, RDMA_PS_TCP )  ) {
        terminal( Connection, "rdma_create_id()\n");
    }

    dbg( Connection, "cm_id=%p context=%p\n", m_cm_id, m_cm_id->verbs );

    struct sockaddr_in addr_in;
    addr_in.sin_addr.s_addr = htonl( m_farId.nid );
    addr_in.sin_family = PF_INET;
    addr_in.sin_port = htons( m_farId.pid );

    dbg( Connection, "addr=%#x\n", addr_in.sin_addr.s_addr );

    pthread_mutex_init( &m_sendQMutex, NULL );

    if ( sem_init(&m_sem,0,0) ) {
        terminal( Connection, "sem_init()\n");
    }

    if ( rdma_resolve_addr( m_cm_id, NULL,
                            (struct sockaddr*) &addr_in, 2000 ) ) {
        terminal( Connection, "rdma_resolve_addr()\n");
    }
    dbg( Connection, "return\n" );
}

inline Connection::Connection( struct ibv_pd* pd,
                                struct ibv_mr* mr,
                                struct ibv_comp_channel* compChan,
                                recvHandler_t& recvHandler,
                                int bufSize,
                                const void* private_data,
                                struct rdma_cm_id* cm_id ) :
    m_state( NewServer ),
    m_compChan( compChan),
    m_pd( pd ),
    m_cm_id( cm_id ),
    m_myId( getMyProcID() ),
    m_recvAckEvent( EventHandler< Connection, bool, struct ibv_wc& >
                                    ( this, &Connection::recvAckEvent ) ),
    m_recvHandler( recvHandler ),
    m_sendCredit( 10 ),
    m_recvCredit( 0 ),
    m_creditThreshold( m_sendCredit/2 ),
    m_bufSize( bufSize ),
    m_foo_mr( mr ),
    m_port( 1 ),
    m_lid( get_local_lid( Connection::m_port ) )
{
    struct rdma_conn_param conn_param;
    int ret;

    srand48( getpid() * time(NULL) );
    m_psn = lrand48() & 0xffffff;

    m_remoteMap.resize(MemMapSize);

    m_mem_mr = ibv_reg_mr( m_pd, &m_remoteMap[0],
        sizeof( m_remoteMap[0] ) * m_remoteMap.size(),
                    ( ibv_access_flags )
                    ( IBV_ACCESS_LOCAL_WRITE|
                    IBV_ACCESS_REMOTE_WRITE ) );

    if ( ! m_mem_mr ) {
        terminal( Connection, "\n" );
    }

    dbg( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );
    dbg( Connection, "cm_id=%p context=%p\n", m_cm_id, m_cm_id->verbs );

    m_cm_id->context = this;

    init();

    PrivateData* tmp = (PrivateData*) private_data;
    m_farId = tmp->procId; 
    m_rkey = tmp->rkey;
    m_raddr = tmp->raddr;

    dbg(Connection,"m_rkey=%#x m_raddr=%#lx\n",m_rkey,m_raddr);
    dbg(Connection,"lid=%x psn=%x qpn=%x\n",
                tmp->qpInfo.lid, tmp->qpInfo.psn, tmp->qpInfo.qpn );

    makeReadyAckQP( tmp->qpInfo, m_psn, m_port ); 

    postRecvs();

    memset(&conn_param, 0, sizeof conn_param);

    conn_param.responder_resources = 1;
    conn_param.initiator_depth = 1;

    PrivateData priv;
    priv.qpInfo.lid = m_lid;
    priv.qpInfo.qpn = m_ackQP->qp_num;
    priv.qpInfo.psn = m_psn;

    priv.rkey = m_foo_mr->rkey;
    priv.raddr = (uint64_t) m_foo_mr->addr;

    dbg(Connection,"rkey=%#x raddr=%p\n",m_foo_mr->rkey,m_foo_mr->addr);

    dbg(Connection,"lid=%x psn=%x qpn=%x\n",
                priv.qpInfo.lid, priv.qpInfo.psn, priv.qpInfo.qpn );

    conn_param.private_data = &priv;
    conn_param.private_data_len = sizeof(priv);

    pthread_mutex_init( &m_sendQMutex, NULL );
    if ( sem_init(&m_sem,0,0) ) {
        terminal( Connection, "sem_init()\n");
    }

    ret = rdma_accept(m_cm_id, &conn_param);
    if (ret) {
        terminal(Connection, "dbgFailure accepting");
    }
}


inline Connection::~Connection()
{
    dbg( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );

    
    int ret;

    rdma_destroy_qp( m_cm_id );

    if ( ibv_destroy_qp( m_ackQP ) ) { 
        terminal( Connection,"ibv_destroy_qp() %s\n", strerror(errno));
    }

    if ( ibv_destroy_cq( m_cq ) ) {
        terminal( Connection,"ibv_destroy_cq() %s\n", strerror(errno));
    }

    if ( ibv_dereg_mr( m_mr ) ) {
        terminal( Connection,"ibv_dereg_mr() %s\n", strerror(errno));
    }

    if ( ibv_dereg_mr( m_mem_mr ) ) {
        terminal( Connection,"ibv_dereg_mr() %s\n", strerror(errno));
    } 

    if ( ( ret = rdma_destroy_id( m_cm_id ) ) ) {
        terminal( Connection,"rdma_destroy_id() error=%d\n",ret);
    }

    sem_destroy(&m_sem);
}

inline void Connection::wait4connect()
{
    dbg( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );

    while ( sem_wait(&m_sem) ) {
        if ( errno == EINTR ) {
            dbg( Connection,"EINTR\n");
        } else {
            throw -1;
        }
    }
    dbg( Connection, "done\n");
}

inline int Connection::disconnect()
{
    int ret;
    dbg( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );
    m_state = Disconnecting;
    ret = rdma_disconnect( m_cm_id );
    if (ret) {
        terminal(Connection, "rdma_disconnect()");
    }
    
    while ( sem_wait(&m_sem) ) {
        if ( errno == EINTR ) {
            dbg( Connection,"EINTR\n");
        } else {
            throw -1;
        }
    }
    return 0;
}

inline void Connection::disconnected()
{ 
    dbg(Connection,"\n");
    struct ibv_qp_attr qp_attr;

    sem_post(&m_sem);

    if (!m_cm_id->qp) {
        terminal(Connection,"invalid qp\n");
    }

    qp_attr.qp_state = IBV_QPS_ERR;
    if ( ibv_modify_qp(m_cm_id->qp, &qp_attr, IBV_QP_STATE) ) {
	// ibv_modify_qp() fails on occasion
	// it doesn't appear to cause any problems
        //dbgFail(Connection,"ibv_modify_qp() %s\n", strerror(errno) );
    }
}

inline int Connection::init(  )
{
    struct ibv_qp_init_attr init_qp_attr;
    int cqe, ret;

    cqe = (m_sendCredit + 1 ) * 2;

    m_cq = ibv_create_cq(m_cm_id->verbs, cqe, this, m_compChan, 0);
    if (!m_cq) {
        terminal( Connection, "unable to create CQ\n");
    }
    if ( ibv_req_notify_cq( m_cq,  0 ) ) {
        terminal( Connection, "ibv_req_notify\n");
    }

    initAckQP();    

    memset(&init_qp_attr, 0, sizeof init_qp_attr);
    init_qp_attr.cap.max_send_wr = m_sendCredit;
    init_qp_attr.cap.max_recv_wr = m_sendCredit;
    init_qp_attr.cap.max_send_sge = 1;
    init_qp_attr.cap.max_recv_sge = 1;
    init_qp_attr.qp_context = this;
    init_qp_attr.sq_sig_all = 0;
    init_qp_attr.qp_type = IBV_QPT_RC;
    init_qp_attr.send_cq = m_cq;
    init_qp_attr.recv_cq = m_cq;

    ret = rdma_create_qp( m_cm_id, m_pd, &init_qp_attr);
    if (ret) {
        terminal( Connection, "unable to create QP\n");
    }
    return 0;
}

inline int Connection::addrResolved( )
{
    dbg( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );
    int ret;

    ret = rdma_resolve_route( m_cm_id, 2000);
dbg(Connection, "\n");
    if (ret) {
        terminal( Connection, "rdma_resove_route()\n");
    }
    return ret;
}

inline int Connection::established( struct rdma_cm_event* event )
{
    dbg( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );

    PrivateData* priv = (PrivateData*) event->param.conn.private_data;

    if ( m_state == NewClient ) { 
        dbg(Connection,"lid=%x psn=%x qpn=%x\n",
                priv->qpInfo.lid, priv->qpInfo.psn, priv->qpInfo.qpn );
        makeReadyAckQP( priv->qpInfo, m_psn, m_port ); 
        m_rkey = priv->rkey;
        m_raddr = priv->raddr;
        dbg(Connection,"m_rkey=%#x m_raddr=%#lx\n",m_rkey,m_raddr);
    }

    m_state = Connected;

    sem_post(&m_sem);

    return 0;
}

inline int Connection::routeResolved( ) 
{
    dbg( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );
    struct rdma_conn_param conn_param;
    int ret;

    init();

    postRecvs();

    memset(&conn_param, 0, sizeof conn_param);

    conn_param.responder_resources = 1;
    conn_param.initiator_depth = 1;

    PrivateData priv;

    priv.qpInfo.lid = m_lid;
    priv.qpInfo.qpn = m_ackQP->qp_num;
    priv.qpInfo.psn = m_psn;

    priv.rkey = m_foo_mr->rkey;
    priv.raddr = (uint64_t) m_foo_mr->addr;

    dbg(Connection,"rkey=%#x raddr=%p\n",m_foo_mr->rkey,m_foo_mr->addr);

    dbg(Connection,"lid=%x psn=%x qpn=%x\n",
                priv.qpInfo.lid, priv.qpInfo.psn, priv.qpInfo.qpn );

    priv.procId = m_myId;
    conn_param.private_data = &priv;
    conn_param.private_data_len = sizeof(priv);

    ret = rdma_connect( m_cm_id, &conn_param);

    if (ret) {
        terminal( Connection, "rdma_connect()\n");
    }
    return 0;
}

inline int Connection::postRecvs()
{
    int size = m_sendCredit * m_bufSize;

    m_mem.resize( size );

    m_mr = ibv_reg_mr( m_pd, &m_mem[0], size, IBV_ACCESS_LOCAL_WRITE );
    if ( ! m_mr ) {
        dbgFail( Connection, "ibv_reg_mr()\n" );
    }

    for ( int i = 0; i < m_sendCredit; i++ )
    {
        struct ibv_recv_wr* recv_wr = new struct ibv_recv_wr;

        _recvHandler_t* handler = 
                new _recvHandler_t(this, &Connection::recvHandler, recv_wr);

        recv_wr->next = NULL;
        recv_wr->sg_list = new struct ibv_sge;
        recv_wr->num_sge = 1;
        recv_wr->wr_id = (uintptr_t) handler;

        recv_wr->sg_list->length = m_bufSize;
        recv_wr->sg_list->lkey = m_mr->lkey;
        recv_wr->sg_list->addr = (uintptr_t) &m_mem[ i * m_bufSize ]; 

        dbg(Connection,"buf=%#lx\n", recv_wr->sg_list->addr );
        if(  ibv_post_recv( m_cm_id->qp, recv_wr, NULL ) ) {
            terminal( Connection, "ibv_post_recv\n");
        }
    } 
    return 0;
}

inline int Connection::read( uint32_t rkey, uint64_t remote_addr, 
                        struct ibv_sge* sge, handler_t* handler ) 
{
    dbg( Connection, "remote_addr=%#lx local_addr=%#lx length=%#x\n",
                        remote_addr, sge->addr, sge->length );

    q_post_wr( sge, handler, IBV_WR_RDMA_READ, 0, rkey, remote_addr );

    return 0;
}

inline int Connection::write( uint32_t rkey, uint64_t remote_addr, 
                        struct ibv_sge* sge, handler_t* handler ) 
{
    dbg( Connection, "remote_addr=%#lx local_addr=%#lx length=%#x\n",
                        remote_addr, sge->addr, sge->length );

    q_post_wr( sge, handler, IBV_WR_RDMA_WRITE, 0, rkey, remote_addr );

    return 0;
}

inline int Connection::send( struct ibv_sge* list, handler_t* handler )
{
    dbg( Connection, "addr=%#lx length=%i\n", list->addr, list->length );

    if ( list->length > m_bufSize ) {
        return -1;
    }
    q_post_wr( list, handler );
    return 0;
}

inline int Connection::q_post_wr( struct ibv_sge* list,
                            handler_t* handler,
                            ibv_wr_opcode opcode,
                            uint32_t imm_data,
                            uint32_t rkey,
                            uint64_t remote_addr )
{
    pthread_mutex_lock( & m_sendQMutex );

    if ( m_sendCredit == 0 ) {  
        SendQEntry entry;
        entry.sge = *list;
        entry.handler = handler;
        entry.opcode = opcode;
        entry.imm_data = imm_data;
        entry.rkey = rkey;
        entry.remote_addr = remote_addr;
        m_sendQ.push_back( entry );
    } else {
        post_wr( list, handler, opcode, imm_data, rkey, remote_addr );
    }

    pthread_mutex_unlock( & m_sendQMutex );

    return 0;
}

inline int Connection::post_wr( struct ibv_sge* list,
                            handler_t* handler, 
                            ibv_wr_opcode opcode,
                            uint32_t imm_data,
                            uint32_t rkey,
                            uint64_t remote_addr )
{
    struct ibv_send_wr wr;
    wr.wr_id = (uintptr_t) handler;
    wr.next = NULL;
    wr.sg_list = list;
    wr.num_sge = 1;
    wr.opcode = opcode;
    wr.send_flags = (ibv_send_flags) IBV_SEND_SIGNALED;
    wr.imm_data = imm_data;

    dbg(Connection,"opcode=%#x flags=%#x imm=%#x\n",
                            wr.opcode, wr.send_flags, wr.imm_data );
    dbg(Connection,"addr=%#lx length=%#x\n",
                            wr.sg_list->addr, wr.sg_list->length );

    if ( opcode == IBV_WR_RDMA_WRITE || 
        opcode == IBV_WR_RDMA_WRITE_WITH_IMM || 
        opcode == IBV_WR_RDMA_READ ) 
    {
        dbg(Connection,"rkey=%#x remote_addr=%#lx\n",rkey,remote_addr);
        wr.wr.rdma.remote_addr = remote_addr;
        wr.wr.rdma.rkey = rkey;
    }

    if ( opcode == IBV_WR_SEND || opcode == IBV_WR_SEND_WITH_IMM )  {
        --m_sendCredit;
        dbg(Connection,"m_sendCredit=%d\n",m_sendCredit);
    }

    struct ibv_send_wr* bad_wr;

    if ( ibv_post_send(m_cm_id->qp, &wr, &bad_wr ) ) {
	terminal(Connection,"ibv_post_send()\n");
    }
    return 0;
}

inline void Connection::makeReadyAckQP( QPInfo& dest, int my_psn, int port )
{
    dbg(Connection,"lid=%#x psn=%x qpn=%x my_psn=%x port=%d\n", 
                    dest.lid, dest.psn, dest.qpn, my_psn, port);
    struct ibv_qp_attr attr; 
    attr.qp_state               = IBV_QPS_RTR;
    attr.path_mtu               = IBV_MTU_1024;
    attr.dest_qp_num            = dest.qpn;
    attr.rq_psn                 = dest.psn;
    attr.max_dest_rd_atomic     = 1;
    attr.min_rnr_timer          = 12;
    attr.ah_attr.is_global      = 0;
    attr.ah_attr.dlid           = dest.lid;
    attr.ah_attr.sl             = 0;
    attr.ah_attr.src_path_bits  = 0,
    attr.ah_attr.port_num       = port;

    if (ibv_modify_qp( m_ackQP, &attr,
                (ibv_qp_attr_mask) (IBV_QP_STATE              |
                                    IBV_QP_AV                 |
                                    IBV_QP_PATH_MTU           |
                                    IBV_QP_DEST_QPN           |
                                    IBV_QP_RQ_PSN             |
                                    IBV_QP_MAX_DEST_RD_ATOMIC |
                                    IBV_QP_MIN_RNR_TIMER))) 
    {
        terminal( Connection, "modify QP to RTR\n");
    }

    attr.qp_state       = IBV_QPS_RTS;
    attr.timeout        = 14;
    attr.retry_cnt      = 7;
    attr.rnr_retry      = 7;
    attr.sq_psn         = my_psn;
    attr.max_rd_atomic  = 1;
    if (ibv_modify_qp(m_ackQP, &attr,
                    (ibv_qp_attr_mask) (IBV_QP_STATE              |
                                        IBV_QP_TIMEOUT            |
                                        IBV_QP_RETRY_CNT          |
                                        IBV_QP_RNR_RETRY          |
                                        IBV_QP_SQ_PSN             |
                                        IBV_QP_MAX_QP_RD_ATOMIC)))
    {
        terminal(Connection, "modify QP to RTS\n");
    }
}

inline void Connection::initAckQP( )
{
    struct ibv_qp_init_attr init_attr;
    memset(&init_attr, 0, sizeof init_attr);
    init_attr.send_cq = m_cq;
    init_attr.recv_cq = m_cq;
    init_attr.qp_context = this;
    init_attr.cap.max_send_wr = 2;
    init_attr.cap.max_recv_wr = 2;
    init_attr.cap.max_send_sge = 1;
    init_attr.cap.max_recv_sge = 1;
    init_attr.qp_type = IBV_QPT_RC;

    m_ackQP = ibv_create_qp(m_pd, &init_attr);
    if (!m_ackQP)  {
        terminal( Connection, "Couldn't create QP\n");
    }

    struct ibv_qp_attr attr;
    attr.qp_state        = IBV_QPS_INIT;
    attr.pkey_index      = 0;
    attr.port_num        = 1;
    attr.qp_access_flags = 0;

    if (ibv_modify_qp( m_ackQP, &attr,
                    (ibv_qp_attr_mask)( IBV_QP_STATE              |
                                        IBV_QP_PKEY_INDEX         |
                                        IBV_QP_PORT               |
                                        IBV_QP_ACCESS_FLAGS)))
    {
        terminal(Connection, "Failed to modify QP to INIT\n");
    }
    postAckRecv();
}
inline int Connection::postAckSend(  uint32_t data )
{
    struct ibv_send_wr send_wr;

    dbg(Connection,"enter\n");

    send_wr.next = NULL;
    send_wr.num_sge = 0;

    send_wr.opcode = IBV_WR_SEND_WITH_IMM;
    send_wr.send_flags = (ibv_send_flags) 0;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.wr_id = (uintptr_t) NULL;

    send_wr.imm_data = data;

    struct ibv_send_wr* bad_wr;
    int ret;
    if ( ( ret = ibv_post_send( m_ackQP, &send_wr, &bad_wr) ) ) {
        terminal( Connection, "ibv_post_send() ret=%d\n", ret );
    }

    return 0;
}

inline int Connection::postAckRecv()
{
    struct ibv_recv_wr recv_wr;

    dbg(Connection,"enter\n");

    recv_wr.next = NULL;
    recv_wr.num_sge = 0;
    recv_wr.wr_id = (uintptr_t) &m_recvAckEvent;

    struct ibv_recv_wr* bad_wr;
    if ( ibv_post_recv( m_ackQP, &recv_wr, &bad_wr) ) {
        terminal(Connection, "ibv_post_recv()\n");
    }
    return 0;
}

inline bool Connection::recvAckEvent( struct ibv_wc& wc)
{
    if ( wc.status != IBV_WC_SUCCESS ) {
        dbgFail( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );
        return false;
    }
    m_sendCredit += wc.imm_data;

    dbg(Connection,"got=%d now m_sendCredit=%d\n",wc.imm_data,m_sendCredit);

    postAckRecv();

    pthread_mutex_lock( &m_sendQMutex );

    while ( ! m_sendQ.empty() && m_sendCredit > 0 ) {
        post_wr( &m_sendQ.front().sge,
                m_sendQ.front().handler,
                m_sendQ.front().opcode,
                m_sendQ.front().imm_data,
                m_sendQ.front().rkey,
                m_sendQ.front().remote_addr);
        m_sendQ.pop_front();
    }
        
    pthread_mutex_unlock( &m_sendQMutex );

    return false;
}

inline bool Connection::recvHandler( struct ibv_wc& wc,
                                            struct ibv_recv_wr* wr  )
{
    if ( wc.status != IBV_WC_SUCCESS ) {
        
        //dbgFail( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );
        return false;
    }

    dbg(Connection,"buf=%#lx\n", wr->sg_list->addr );

    m_recvHandler( (void*) wr->sg_list->addr, *this ); 

    struct ibv_recv_wr* bad_wr;
    if(  ibv_post_recv( m_cm_id->qp, wr, &bad_wr ) ) {
        terminal( Connection, "ibv_post_recv\n");
    }

    ++m_recvCredit;
    dbg(Connection,"m_recvCredit=%d\n",m_recvCredit);
    if ( m_recvCredit == m_creditThreshold ) {
        postAckSend( m_recvCredit );
        m_recvCredit = 0;
    }  
    
    return  false;
}

inline  RemoteEntry* Connection::remoteEntry( MemRegion::Id id )
{
    if ( m_remoteMap[id].key == 0 ) {

        sem_t   sem;
        if ( sem_init( &sem, 0, 0 ) ) {
            terminal( Connection, "sem_init()\n");
        }
        dbg( Connection,"get id=%d\n",id);
        memHandler_t* handler =
            new memHandler_t( this, &Connection::memHandler, &sem );

        struct ibv_sge sge;
        sge.addr = (uint64_t) &m_remoteMap[0];
        sge.length = sizeof( m_remoteMap[0] ) * m_remoteMap.size();
        sge.lkey = m_mem_mr->lkey;

        dbg(Connection,"rkey=%#x raddr=%#lx\n",m_rkey,m_raddr);
        read( m_rkey, m_raddr, &sge, handler ); 

        while ( sem_wait(&sem) ) {
            if ( errno == EINTR ) {
                dbg( Connection,"EINTR\n");
            } else {
                throw -1;
            }
        }
        sem_destroy( &sem );

        if ( m_remoteMap[id].key == 0 ) {
            return NULL;
        }
    }     

    dbg( Connection,"id=%d\n",id);
     
    return &m_remoteMap[id];
}

inline bool Connection::memHandler( struct ibv_wc& wc, sem_t *sem )
{
    if ( wc.status != IBV_WC_SUCCESS ) {
        dbgFail( Connection, "[%#x,%d]\n", m_farId.nid, m_farId.pid );
        return true;
    }

    dbg(Connection,"\n");
    sem_post(sem);
    return true;
}

} // namespace rdmaPlus
#endif

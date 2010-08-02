
#ifndef _workComp_h
#define _workComp_h

#include <pthread.h>
#include <util.h>
#include <connection.h>
#include <eventHandler.h>

class WorkComp {
    public:
        WorkComp( struct ibv_context* ); 
        ~WorkComp(); 

        struct ibv_comp_channel* compChan() {
            return m_channel;
        }

    private:
        static void* thread( void* obj );
        void* thread( );
        int eventHandler( struct ibv_wc& );

        pthread_t           m_thread;

        struct ibv_comp_channel*    m_channel;
};

inline WorkComp::WorkComp( struct ibv_context* verbs )
{
    dbg( WorkCOmp, "enter verbs=%p\n", verbs );

    if( ! ( m_channel = ibv_create_comp_channel( verbs ) ) ) {
        terminal( WorkComp, "ibv_create_comp_channel()\n");
    }

    dbg( WorkCOmp, "verbs=%p %p\n", verbs, m_channel );

    if ( pthread_create( &m_thread, NULL, thread, this ) ) {
        terminal( rdma, "pthread_create()\n");
    }
}

inline WorkComp::~WorkComp()
{
    dbg( WorkComp, "enter\n" );

    if ( pthread_cancel( m_thread ) ) {
        terminal( WorkComp, "pthread_cancel()\n");
    }

    if ( pthread_join( m_thread, NULL ) ) {
        terminal( WorkComp, "pthread_join()\n");
    }

    if ( ibv_destroy_comp_channel( m_channel ) ) {
        terminal( WorkComp, "ibv_destroy_comp_channel()\n");
    }
    dbg( WorkComp, "return\n" );
}


inline int WorkComp::eventHandler( struct ibv_wc& wc )
{
    dbg(WorkComp,"opcode=%#x `%s` bytes=%d wr_id=%#lx\n", 
           wc.opcode, ibv_wc_status_str(wc.status), wc.byte_len,wc.wr_id );

    if ( wc.wr_id ) {
        EventHandlerBase<bool,struct ibv_wc&>* handler =
                        (EventHandlerBase<bool,struct ibv_wc&>*)wc.wr_id;

        if ( (*handler)(wc) ) {
            dbg( WorkComp, "delete handler\n");
            delete handler;
        }
    }
    
    return 0;
}

inline void* WorkComp::thread( void* obj ) {
    return ( (WorkComp*) obj)->thread();
}

inline void* WorkComp::thread()
{
    struct ibv_cq *ev_cq;
    void *ev_ctx;
    int ret;

    dbg( WorkComp, "\n");

    while (1) {

	//printf("calling ibv_get_cq_evetn()\n");
        if ( ibv_get_cq_event(m_channel, &ev_cq, &ev_ctx) ) {
            dbgFail(WorkComp, "Failed to get cq event!\n");
            pthread_exit(NULL);
        }

        if (ibv_req_notify_cq(ev_cq, 0) ) {
            terminal(WorkComp, "Failed to set notify!\n");
        }

        ibv_wc wc[50];

        while ( ( ret = ibv_poll_cq( ev_cq, 50, wc ) ) > 0 ) {
            for ( int i = 0; i < ret; i++ ) {
                eventHandler( wc[i] );
            }
        } 
        ibv_ack_cq_events( ev_cq, 1 );
    }
    return NULL;
}

#endif

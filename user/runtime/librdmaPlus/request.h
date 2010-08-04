#ifndef _rdma_request_h
#define _rdma_request_h

#include "./types.h"
#include <msg.h>
#include <semaphore.h>
#include <errno.h>

namespace rdmaPlus {

class Request {
    public:
	    typedef bool (*callback_t)(void*,void*);

	    Request( callback_t func = NULL, void* obj=NULL, void* data = NULL ) :
            m_mr( NULL ),
		    m_cbFunc( func ),
		    m_cbObj( obj ),
		    m_cbData( data )
	    {
            dbg(Reqeust,"enter\n");
            sem_init(&m_sem,0,0);
	    }

	    Request& init( struct ibv_pd* pd, void* addr,
                                Size length, ProcId& id, Tag tag )
	    {
  	        m_key.count = length;
	        m_key.id = id;
	        m_key.tag = tag;
		    m_buf = addr;

		    dbg(Request,"this=%p addr=%p length=%lu tag=%#x\n",
					this, addr, length, tag );

		    if ( length > Msg::ShortMsgLength ) {
                m_mr = ibv_reg_mr( pd, addr, length, 
                            (ibv_access_flags)
                            (IBV_ACCESS_LOCAL_WRITE |
                            IBV_ACCESS_REMOTE_READ |
                            IBV_ACCESS_REMOTE_WRITE));
                if ( ! m_mr ) {
                    terminal( Request, "ibv_reg_mr()\n");
                } 
		        dbg(Request,"lkey=%#x rkey=%#x\n", m_mr->lkey, m_mr->rkey );
		    }
		    return *this;
	    }

	    ~Request()
	    {
            dbg(Request,"enter\n");
            if ( m_mr ) {
                if ( ibv_dereg_mr( m_mr ) ) {
                    terminal(Request,"ibv_dereg_mr()\n");
                }
            }
            sem_destroy(&m_sem);
	    }

	    bool wake( )
	    {
       	        bool ret = callback();
                sem_post(&m_sem);
       	        return ret;
	    }

	    int wait( long time, Status* status )
	    {
            if ( time == -1 ) {
                while ( sem_wait(&m_sem) ) {
                    if ( errno == EINTR ) {
                        dbg( Request,"EINTR\n");
                    } else {
                        throw -1;
                    }
                }

            } else if ( time == 0 ) {
                if ( sem_trywait( &m_sem ) ) {
                    return 0;
                }
            } else {
                terminal(Request,"not implemented\n");
            }
            if( status ) {
                status->key = m_key;
            }
       	    return 1;
	    }

	    Key& key() {
		    return m_key;
	    }

	    void* buf() {
		    return m_buf;
	    }
    
        Size length() {
            return m_key.count;
        }

	    void setCbData( void* data ) {
		    m_cbData = data;
	    }

        uint32_t lkey() {
            if ( m_mr ) {
                return m_mr->lkey;
            } else {
                return 0;
            }
        } 

        uint32_t rkey() {
            if ( m_mr ) {
                return m_mr->rkey;
            } else {
                return 0;
            }
        } 


    private:
	    bool callback() {
                bool ret = false;
                dbg(Request,"callback=%p\n",m_cbFunc);
                if ( m_cbFunc ) {
                    ret = m_cbFunc( m_cbObj, m_cbData );
                }
                return ret;                  	 
	    }

        struct ibv_mr* m_mr;
	    void*		m_buf;
	    Key 		m_key;
	    callback_t	m_cbFunc;
	    void*		m_cbObj;
	    void*		m_cbData;
        sem_t       m_sem;
};

} // namespace rdmaPlus

#endif

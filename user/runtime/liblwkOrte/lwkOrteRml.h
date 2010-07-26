#ifndef _LWK_ORTE_RML_H
#define _LWK_ORTE_RML_H

#include <pthread.h>
#include "orte_config.h"
#include "orte/orte_constants.h"
#include "orte/mca/rml/base/base.h"
#include <map>
#include <list>
#include <semaphore.h>

struct LwkOrteRmlMsg;

#if 0  
#define Debug( name, fmt, args... ) \
    printf( "%s::%s():%d: "fmt, #name,__FUNCTION__,__LINE__, ## args )
#else
#define Debug( name, fmt, args... )
#endif

class LwkOrteRml {
        struct RecvEntry {
        	orte_process_name_t peer;
        	orte_rml_tag_t tag;
        	int flags;
        	orte_rml_buffer_callback_fn_t cbfunc;
        	void *cbdata;
        };

        struct SendEntry {
        	orte_process_name_t peer;
        	orte_buffer_t * buffer;
        	orte_rml_tag_t tag;
        	int flags;
        	orte_rml_buffer_callback_fn_t cbfunc;
        	void *cbdata;
        };

    public:

        LwkOrteRml();
        ~LwkOrteRml();
        void init( orte_process_name_t* );
        int  recvBufferNB( orte_process_name_t * peer, orte_rml_tag_t tag,
                int flags, orte_rml_buffer_callback_fn_t cbfunc, void *cbdata);
        int  recvNB( orte_process_name_t * peer, orte_rml_tag_t tag,
                int flags, orte_rml_buffer_callback_fn_t cbfunc, void *cbdata);
        int sendBufferNB( orte_process_name_t * peer, orte_buffer_t * buffer,
                orte_rml_tag_t tag, int flags,
                orte_rml_buffer_callback_fn_t cbfunc, void *cbdata);
        int barrier();
        int recvCancel( orte_process_name_t* peer, orte_rml_tag_t tag );

    private:

        typedef std::multimap<orte_rml_tag_t,RecvEntry*> recvEntryMap_t;
        typedef std::list< std::pair< LwkOrteRmlMsg*, unsigned char*> > foo_t;

    	static void* thread( void* );
    	void* thread();
    	void processRead(int);
    	void processMsg( LwkOrteRmlMsg*, unsigned char* );

        void completeRecv( orte_rml_buffer_callback_fn_t, void*,
                                LwkOrteRmlMsg* hdr, unsigned char* body );

        int                     m_wFd;
        int                     m_rFd;
        pthread_mutex_t         m_qLock;
        pthread_mutex_t         m_fdLock;
        sem_t                   m_sem;
        pthread_t	            m_pthread;

        recvEntryMap_t	        m_postedRecvM;
        foo_t 		            m_recvdL;

        LwkOrteRmlMsg*	        m_readMsgPtr;
	
        orte_process_name_t*    m_myname;
        bool                    m_threadRun;   
    
        int                     m_dbg_count;
};

#endif

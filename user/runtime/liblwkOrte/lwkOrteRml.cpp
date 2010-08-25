
#include <sstream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string>

#include <lwkOrteRml.h>
#include <lwkOrteRmlMsg.h>

#include "orte/mca/oob/base/base.h"
#include "orte/dss/dss.h"

#define Error( name, fmt, args... ) \
    printf( "ERROR:%s::%s():%d: "fmt, #name,__FUNCTION__,__LINE__, ## args )


LwkOrteRml::LwkOrteRml() :
	m_readMsgPtr( NULL ),
	m_threadRun( true ),
    m_dbg_count( 0 )
{
    Debug(LwkOrteRml,"Enter\n" );

    pthread_mutex_init( &m_qLock, NULL );
    pthread_mutex_init( &m_fdLock, NULL );
    sem_init(&m_sem,0,0);

    std::string filename;
    std::ostringstream dir;
    dir << "/proc/" << getpid();
    filename = dir.str() + "/pctFifo";

    Debug( LwkOrteRml, "open(`%s` )\n", filename.c_str() );

    if ( ( m_wFd = open( filename.c_str(), O_WRONLY ) ) == -1 )  {
        Error( LwkOrteRml, "open( `%s`, O_WRONLY ) failed\n", filename.c_str());
    	throw -1;
    }

    filename = dir.str() + "/appFifo";
    Debug( LwkOrteRml, "open(`%s` )\n", filename.c_str() );

    if ( (  m_rFd = open( filename.c_str(), O_RDONLY ) ) == -1 ) {
        Error( LwkOrteRml, "open( `%s`, O_RDONLY ) failed\n", filename.c_str());
	    throw -1;
    }

    if (  pthread_create( &m_pthread, NULL, thread, this ) ) {
        Error( LwkOrteRml, "pthread_create() failed\n" );
        throw -1;
    }

    Debug(LwkOrteRml,"Return\n");
}

LwkOrteRml::~LwkOrteRml()
{
    Debug(LwkOrteRml,"Enter\n");

    m_threadRun = false;
    if ( pthread_cancel( m_pthread ) ) {
       	Error( LwkOrteRml, "pthread_cancel() failed\n" );
        throw -1;
    }

    if ( pthread_join( m_pthread, NULL ) ) {
        Error( LwkOrteRml, "pthread_join() failed\n" );
        throw -1;
    }

    if ( close( m_wFd ) ) {
        Error( LwkOrteRml, "close( %d ) failed\n", m_wFd );
        throw -1;
    }

    if ( close( m_rFd ) ) {
        Error( LwkOrteRml, "close( %d ) failed\n", m_rFd );
        throw -1;
    }
    sem_destroy(&m_sem);
    Debug(LwkOrteRml,"Return\n");
}

void LwkOrteRml::init( orte_process_name_t* myname )
{
	Debug(LwkOrteRml,"[%d,%d,%d]\n",
				myname->cellid,myname->jobid,myname->vpid);
	m_myname = myname;
}

int  LwkOrteRml::recvBufferNB( orte_process_name_t * peer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t cbfunc,
				void *cbdata)
{
	Debug( LwkOrteRml, "cellid=%d jobid=%d vpid=%d tag=%x flags=%x\n",
                peer->cellid,peer->jobid,peer->vpid, tag, flags);

	if ( flags & ( MCA_OOB_PEEK | MCA_OOB_TRUNC | MCA_OOB_ALLOC ) ) {
		printf("%s():%d failed\n",__FUNCTION__,__LINE__);
		return ORTE_ERR_NOT_SUPPORTED;	
	} 

	pthread_mutex_lock( &m_qLock ); 

	LwkOrteRmlMsg* msg;
	unsigned char* body = NULL;

	foo_t::iterator iter = m_recvdL.begin();
	for ( ; iter != m_recvdL.end(); iter++ )
	{
		if ( tag != (orte_rml_tag_t) -1 && tag != msg->u.oob.tag )
			continue;  
		if ( peer->cellid != -1 && peer->cellid != msg->myname.cellid )
			continue;  
		if ( peer->jobid != -1 && peer->jobid != msg->myname.jobid )
			continue;  
		if ( peer->vpid != -1 && peer->vpid != msg->myname.vpid )
			continue;  
		body = iter->second;
		m_recvdL.erase( iter );
	}

	if ( body ) {
		Debug( LwkOrteRml, "found\n" );
		completeRecv( cbfunc, cbdata, msg, body );
	} else {
		Debug( LwkOrteRml, "not found\n" );

		RecvEntry* entry = new RecvEntry;

		entry->peer = *peer;
		entry->tag = tag;
		entry->flags = flags;
		entry->cbfunc = cbfunc;
		entry->cbdata = cbdata;

		m_postedRecvM.insert(
			std::pair< orte_rml_tag_t, RecvEntry* >( tag, entry ) );
	}

	pthread_mutex_unlock( &m_qLock ); 

	return ORTE_SUCCESS;
}
	
int  LwkOrteRml::recvNB( orte_process_name_t * peer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t cbfunc,
				void *cbdata)
{
	Debug( LwkOrteRml, "cellid=%d jobid=%d vpid=%d tag=%x flags=%x\n",
                peer->cellid,peer->jobid,peer->vpid, tag, flags);

	return ORTE_SUCCESS;
}
	


int LwkOrteRml::sendBufferNB( orte_process_name_t * peer,
                               orte_buffer_t * buffer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t cbfunc,
				void *cbdata)
{
	Debug( LwkOrteRml, "cellid=%d jobid=%d vpid=%d tag=%x flags=%x\n",
                peer->cellid,peer->jobid,peer->vpid, tag, flags);

	if ( flags ) {
		printf("%s():%d failed\n",__FUNCTION__,__LINE__);
		return ORTE_ERR_NOT_SUPPORTED;	
	} 
	LwkOrteRmlMsg msg;
	msg.type = LwkOrteRmlMsg::OOB;
	msg.myname = *m_myname;
	msg.u.oob.peer = *peer;
	msg.u.oob.tag = tag;

	void* buf;
	orte_dss.unload( buffer, &buf, &msg.u.oob.nBytes );
	
	// make this non-blocking
    pthread_mutex_lock( &m_fdLock );
    if ( write( m_wFd, &msg, sizeof( msg ) ) != sizeof(msg) ) {
        abort();
	}
	
    if ( write( m_wFd, buf, msg.u.oob.nBytes ) != msg.u.oob.nBytes ) {
        abort();
    }
    pthread_mutex_unlock( &m_fdLock );

    Debug(LwkOrteRml,"before cbfunc=%p cbdata=%p buffer=%p\n",
                                                cbfunc, cbdata, buffer );
    cbfunc( 0, peer, buffer, tag, cbdata );
    Debug(LwkOrteRml,"after cbfunc\n");
	return ORTE_SUCCESS;
}

int LwkOrteRml::barrier()
{
	Debug( LwkOrteRml, "enter\n" );

	LwkOrteRmlMsg msg;
	msg.type = LwkOrteRmlMsg::BARRIER;
	msg.myname = *m_myname;
	msg.u.oob.nBytes = 0;     

	pthread_mutex_lock( &m_fdLock );
	if ( write( m_wFd, &msg, sizeof(msg) ) != sizeof(msg)) {
		abort();
	}
	pthread_mutex_unlock( &m_fdLock );

	Debug( LwkOrteRml, "waiting for response\n" );

    sem_wait(&m_sem);

	Debug( LwkOrteRml, "returning\n" );
	return ORTE_SUCCESS;
}

int LwkOrteRml::recvCancel( orte_process_name_t* peer, orte_rml_tag_t tag )
{
	Debug ( LwkOrteRml, "[%d,%d,%d] tag=%#x\n", 
				peer->cellid, peer->jobid, peer->vpid, tag );

	pthread_mutex_lock( &m_qLock );

	std::pair<recvEntryMap_t::iterator,recvEntryMap_t::iterator> iterPair;
	iterPair = m_postedRecvM.equal_range( tag );

	for ( ; iterPair.first != iterPair.second; iterPair.first++ )
	{
		RecvEntry* entry = iterPair.first->second;
		if ( peer->cellid != entry->peer.cellid )
			continue;  
		if ( peer->jobid != entry->peer.jobid )
			continue;  
		if ( peer->vpid != entry->peer.vpid )
			continue;  

		Debug( LwkOrteRml, "found match\n" );
		delete iterPair.first->second;

		m_postedRecvM.erase( iterPair.first );
		pthread_mutex_unlock(&m_qLock);
		return ORTE_SUCCESS;
	}
	pthread_mutex_unlock(&m_qLock);

	return ORTE_ERR_NOT_FOUND;
}

void* LwkOrteRml::thread( void* ptr) {
	return ((LwkOrteRml*)ptr)->thread();
}

void* LwkOrteRml::thread( )
{
    struct pollfd fds[1];
    Debug(LwkOrteRml,"entered\n");

    fds[0].fd = m_rFd;
    fds[0].events = POLLIN;

    while ( m_threadRun ) {
        int __attribute__((unused)) ret; 
        if ( ( ret = poll( fds, 1, -1 ) ) > 0 ) {
            if ( fds[0].revents & POLLIN ) {
                processRead( fds[0].fd );
            }
        } else {
		    Debug(LwkOrteRml,"poll() returned %d\n",ret);
        }
    }
    Debug(LwkOrteRml,"returning\n");
    return NULL;
}

void LwkOrteRml::processRead( int fd)
{
    // we should not need to put a lock around the read fd but it tests
    // indicate we do, verify this 
    pthread_mutex_lock( &m_fdLock );

    m_readMsgPtr = new LwkOrteRmlMsg;

    if ( read( m_rFd, m_readMsgPtr, sizeof( *m_readMsgPtr ) ) != 
                                        sizeof( *m_readMsgPtr) ) {
        abort();
    }

    unsigned char* body;
    if ( m_readMsgPtr->type == LwkOrteRmlMsg::BARRIER ) {
        pthread_mutex_unlock( &m_fdLock );
        Debug(LwkOrteRml,"wake barrier\n");
        sem_post(&m_sem);
        delete m_readMsgPtr;
    } else {
        // this needs to be a malloc because the pointer 
        // gets passed to a orte_buffer_t.
        body = (unsigned char*) malloc( m_readMsgPtr->u.oob.nBytes );

        Debug(LwkOrteRml,"got body %p\n", body);
        if ( read( m_rFd, body, m_readMsgPtr->u.oob.nBytes ) != 
                                    m_readMsgPtr->u.oob.nBytes ) {
            abort();
        }
        pthread_mutex_unlock( &m_fdLock );

        processMsg( m_readMsgPtr, body );
    }
}

void LwkOrteRml::processMsg( LwkOrteRmlMsg* hdr, unsigned char * body )
{
	Debug( LwkOrteRml, "enter\n" );

	pthread_mutex_lock( &m_qLock );

	std::pair<recvEntryMap_t::iterator,recvEntryMap_t::iterator> iterPair;
	iterPair = m_postedRecvM.equal_range( hdr->u.oob.tag );

	RecvEntry* entry = NULL;
	for ( ; iterPair.first != iterPair.second; iterPair.first++ )
	{
		Debug( LwkOrteRml, "found match\n" );
		entry = iterPair.first->second;

		if ( entry->peer.cellid != -1 && 
				entry->peer.cellid != hdr->myname.cellid )
			continue;  
		if ( entry->peer.jobid != -1 && 
				entry->peer.jobid != hdr->myname.jobid )
			continue;  
		if ( entry->peer.vpid != -1 &&
				entry->peer.vpid != hdr->myname.vpid )
			continue;  

		if ( ! ( entry->flags & MCA_OOB_PERSISTENT ) )  {
			m_postedRecvM.erase( iterPair.first );
		}

		completeRecv( entry->cbfunc, entry->cbdata, hdr, body );
		if ( ! ( entry->flags & MCA_OOB_PERSISTENT ) )  {
			Debug( LwkOrteRml, "delete recv entry\n" );
			delete entry;
		}
		pthread_mutex_unlock( &m_qLock );
		return;
	}

	Debug( LwkOrteRml, "no match\n" );
	m_recvdL.push_back(
		    std::pair< LwkOrteRmlMsg*, unsigned char* >( hdr, body ) );
	pthread_mutex_unlock(&m_qLock);
}

void LwkOrteRml::completeRecv( orte_rml_buffer_callback_fn_t cbfunc,
                                void *cbdata,
				LwkOrteRmlMsg* hdr, 
				unsigned char* body ) 
{
	orte_buffer_t buffer;

	Debug( LwkOrteRml, "[%d,%d,%d] tag=%#x nBytes=%i\n", 
			hdr->myname.cellid, hdr->myname.jobid, hdr->myname.vpid,
			hdr->u.oob.tag, hdr->u.oob.nBytes );

	OBJ_CONSTRUCT( &buffer, orte_buffer_t );
	orte_dss.load( &buffer, body, hdr->u.oob.nBytes );

	Debug( LwkOrteRml, "before cbfunc=%p cbdata=%p\n", cbfunc, cbdata );
	cbfunc( 0, &hdr->myname, &buffer, hdr->u.oob.tag, cbdata );
	Debug( LwkOrteRml, "after\n" );

	OBJ_DESTRUCT( &buffer );

	// note that body is free'd via OBJ_DESTRUCT

	delete hdr;
}

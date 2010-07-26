
#include <sstream>
#include <fstream>
#include <sys/syscall.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pct/debug.h>
#include <pct/orte.h>
#include <pct/msgs.h>
#include <pct/route.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <lwkOrteRmlMsg.h>


Orte::Orte( Rdma& dm, Route& route, int baseRank, int ranksPer, 
                                    std::vector< ProcId >& nidMap ) :
	m_localNidMap( nidMap ),
	m_dm( dm ),	
	m_route( route ),
    m_baseRank( baseRank ),
    m_ranksPer( ranksPer ),
	m_numChildren( m_localNidMap.size() - 1 ),
	m_barrierPendingCnt(  m_numChildren + ranksPer ),
#if 0
    m_log( "Orte::" ),
#endif
	m_barrierRoot( baseRank == 0 ? true : false )
{
    Debug( Orte, "baseRank=%d ranksPer=%d\n", baseRank, ranksPer );
	Debug( Orte, "root=%d numChildren=%d pending=%d\n",
                    m_barrierRoot, m_numChildren, m_barrierPendingCnt );
	armRecv();
    pthread_mutex_init(&m_mutex,NULL);
}

Orte::~Orte()
{
	Debug(Orte,"enter\n");

	Key key = { { -1, -1 }, ORTE_MSG_TAG, sizeof(m_msg) };
	Request* req;

	if ( ( req = m_dm.recvCancel( key ) ) ) {
		Debug( Orte, "canceled Request=%p\n", req );
		delete req;
	}
	Debug(Orte,"return\n");

    //m_log.dump();
}


void Orte::startThread()
{
    m_threadAlive = true;
    if ( pthread_create( &m_thread, NULL, thread, this ) ) {
        Warn(Orte,"pthread_create() failed\n");
        throw -1;
    }
}

void Orte::stopThread()
{
    m_threadAlive = false;
    Debug(Orte,"entered\n");

    if ( pthread_cancel( m_thread ) ) {
        Warn(Orte,"pthead_cancel() failed %s\n", strerror(errno) );
        throw -1;
    }

    if ( pthread_join( m_thread, NULL ) ) {
        Warn(Orte,"pthead_join() failed %s\n", strerror(errno));
        throw -1;
    }
    Debug(Orte,"returning\n");
}

void* Orte::thread( void* obj )
{
    ((Orte*)obj)->thread();    
    return NULL;
}

void Orte::initPid( int pid, int totalRanks, int myRank )
{
    std::string filename;
    std::ostringstream dir;
    dir << "/proc/" << pid;

    m_rank2PidM[myRank] = pid;

    if ( mkdir( dir.str().c_str(), 0666 ) ) {
        Warn(App,"mkdir(%s) failed: %s\n", dir.str().c_str(), strerror(errno));
        throw -1;
    }

    filename = dir.str() +"/app-info";
    Debug(Job,"open %s\n", filename.c_str() );

    std::ofstream file( filename.c_str() );
    file << totalRanks << std::endl;
    file << myRank << std::endl;
    file.close();

    filename = dir.str() +"/pctFifo";
	Debug( Orte, "mknod(%s)\n", filename.c_str()  );
    if  ( mknod( filename.c_str(), 0666, 0 ) ) {
        Warn(App,"mknod(%s) failed: %s\n", filename.c_str(),strerror(errno));
        throw -1;
    }

    if ( ( m_pid2fdM[pid].first = open( filename.c_str(), O_RDONLY ) ) == -1 ) {
        Warn(App,"open(%s) failed: %s\n", filename.c_str(), strerror(errno) );
        throw -1;
    }
	Debug( Orte, "open(%s) = %d\n", filename.c_str(), m_pid2fdM[pid].first  );

    filename = dir.str() +"/appFifo";
	Debug( Orte, "mknod(%s)\n", filename.c_str()  );
    if ( mknod( filename.c_str(), 0666, 0 ) ) {
        Warn(App,"mknod(%s) failed: %s\n", filename.c_str(),strerror(errno));
        throw -1;
    }

    if ( ( m_pid2fdM[pid].second = open( filename.c_str(), O_WRONLY ) ) == -1 ){
        Warn(App,"open(%s) failed: %s\n", filename.c_str(), strerror(errno) );
        throw -1;
    }
	Debug( Orte, "open(%s) = %d\n", filename.c_str(), m_pid2fdM[pid].second  );
}

void Orte::finiPid( int pid )
{
	close( m_pid2fdM[pid].first );
	close( m_pid2fdM[pid].second );

    std::string filename;
    std::ostringstream dir;
    dir << "/proc/" << pid;

    filename = dir.str() +"/app-info";
	Debug( Orte, "unlink(%s)\n", filename.c_str()  );
    if ( unlink( filename.c_str() ) ) {
        Warn(App,"unlink(%s) failed: %s\n", filename.c_str(),strerror(errno));
        throw -1;
    }

    filename = dir.str() +"/pctFifo";
	Debug( Orte, "unlink(%s)\n", filename.c_str()  );
    if ( unlink( filename.c_str() ) ) {
        Warn(App,"unlink(%s) failed: %s\n", filename.c_str(),strerror(errno));
        throw -1;
    }

    filename = dir.str() +"/appFifo";
	Debug( Orte, "unlink(%s)\n", filename.c_str()  );
    if ( unlink( filename.c_str() ) ) {
        Warn(App,"unlink(%s) failed: %s\n", filename.c_str(),strerror(errno));
        throw -1;
    }

    if ( rmdir( dir.str().c_str() ) ) {
        Warn(App,"unlink(%s) failed: %s\n", dir.str().c_str(), strerror(errno));
        throw -1;
    }
}

void Orte::armRecv( )
{
    ProcId  id = { -1, -1 };

	Request* req = new Request( recvCallback, (void*) this );
	req->setCbData( req );

	Debug(Orte,"request=%p\n", req );

	// should dynamically allocate m_msg, as it stands we only post one
	// recv at a time and a single receive buffer works, however if for
	// some reason we want more posted receives this will not work 
    if ( m_dm.irecv( &m_msg, sizeof(m_msg), id, ORTE_MSG_TAG, *req ) > 0 )
	{
        printf("irecv failed\n");
        exit(-1);
    }
	Debug(Orte,"returning\n" );
}

void Orte::thread( )
{
    std::vector<struct pollfd> fds( m_pid2fdM.size() * 2 );

    Debug(Orte," %lu \n",m_pid2fdM.size());
    std::map< int, std::pair<int,int> >::iterator   iter = m_pid2fdM.begin();

    int nfds = 0;
    for ( ; iter != m_pid2fdM.end(); ++iter ) {
        Debug(Orte,"pid=%d fd=%d fd=%d\n",
                        iter->first, iter->second.first, iter->second.second);
        fds[nfds].fd = iter->second.first;
        fds[nfds].events = POLLIN;
        ++nfds;
#if 0
        fds[nfds].fd = iter->second.second;
        fds[nfds].events = POLLOUT;
        ++nfds;
#endif
    }

    int ret;
    Debug(Orte,"nfds=%d\n",nfds);
    while ( m_threadAlive ) {
	//printf("Orte calling poll\n");
        if ( ( ret = poll( &fds[0], nfds, -1 ) ) > 0 ) {
            for ( int i = 0; i < nfds; i++ ) { 
                if ( fds[i].revents & POLLIN ) {
                    processRead( fds[i].fd );
                }
                if ( fds[i].revents & POLLOUT ) {
                    processWrite( fds[i].fd );
                }
            }
        } else {
            Debug(Orte,"poll() returned %d\n", ret );
        }
    }
    Debug(Orte,"thread exiting\n");
}

void* Orte::recvCallback( void* obj, void *data ) 
{
	return ( ( Orte*)obj)->recvCallback( (Request*) data );
}

void* Orte::recvCallback( Request* req  ) 
{
	Debug( Orte, "req=%p\n", req );
    Debug( Orte, "type=%d\n", m_msg.type );

	switch ( m_msg.type ) {
		case LwkOrteRmlMsg::BARRIER:
			processBarrierMsg( &m_msg );
			break;
		case LwkOrteRmlMsg::OOB:
			processOOBMsg( &m_msg );
			break;
	}
	delete req;
	armRecv();
	return NULL;
}

void Orte::processOOBMsg( OrteMsg* orteMsg )
{
    LwkOrteRmlMsg* msg = (LwkOrteRmlMsg*) orteMsg->u.oob.data;	
    size_t nbytes = sizeof(*msg) + msg->u.oob.nBytes;

    Debug( Orte, "vpid=%d tag=%#x nBytes=%d\n",
            msg->u.oob.peer.vpid, msg->u.oob.tag, msg->u.oob.nBytes );

    if ( msg->u.oob.peer.vpid >= m_baseRank &&
                msg->u.oob.peer.vpid < m_baseRank + m_ranksPer ) { 


#if 0
        m_log.write( "%d:%s:%d: recvd from %d\n",pthread_self(),
                                __func__,__LINE__, msg->myname.vpid);
#endif
	    // should this be non-blocking?
        int fd = m_pid2fdM[ m_rank2PidM[msg->u.oob.peer.vpid] ].second;
pthread_mutex_lock(&m_mutex);
	    if ( (size_t) write( fd, msg, nbytes ) != nbytes ) {
            Debug( Orte, " write failed\n" );
            exit(-1);
	    }
pthread_mutex_unlock(&m_mutex);

    } else {

        ProcId dest; 

        dest.nid = m_route.rank2nid( msg->u.oob.peer.vpid );
        dest.pid = m_dm.pid();

        Debug( Orte, "forward to [%#x,%d]\n", dest.nid, dest.pid );

#if 0
        m_log.write( "%d:%s:%d: forward rank=%d\n", pthread_self(),
                __func__,__LINE__, msg->u.oob.peer.vpid);
#endif

        sendMsg( orteMsg, dest );
    }
}

void* Orte::sendCallback( void* obj, void *data ) 
{
	return ( ( Orte*)obj)->sendCallback( (sendCbData_t*) data );
}

void* Orte::sendCallback( sendCbData_t* data  ) 
{
	Debug( Orte, "enter\n");

	// Request*
	delete data->first;

	// OrteMsg*
	delete data->second;

	//std::pair<Request*,void*>
	delete data;
	return NULL;
}

void Orte::processRead( int fd )
{
    Debug( Orte, "fd=%d\n", fd );
    LwkOrteRmlMsg*    readMsgPtr = new LwkOrteRmlMsg;

    if ( read( fd, readMsgPtr, sizeof( *readMsgPtr ) ) !=
                                        sizeof( *readMsgPtr) ) {

        Warn(Orte,"read() hdr failed\n");
        abort();
    }
    Debug(Orte,"got msg nbytes=%d \n",readMsgPtr->u.oob.nBytes);
    if ( readMsgPtr->type == LwkOrteRmlMsg::BARRIER ) {
        delete readMsgPtr;
        barrier(  NULL );
        return;
    }

    unsigned char* body = new unsigned char[ readMsgPtr->u.oob.nBytes ];
    if ( read( fd, body, readMsgPtr->u.oob.nBytes ) !=
                                        readMsgPtr->u.oob.nBytes ) {
        Warn(Orte,"read() body failed\n");
        abort();
    }
    sendOOBMsg( readMsgPtr, body );
}

void Orte::processWrite( int fd )
{
    //Debug( Orte, "fd=%d\n", fd );
}

void Orte::barrier( LwkOrteRmlMsg* msg )
{
    Debug( Orte, "%lu\n", m_localNidMap.size() );

    --m_barrierPendingCnt;

#if 0
     m_log.write( "%d:%s:%d:\n",pthread_self(),__func__,__LINE__);
#endif

    if ( m_barrierPendingCnt == 0 ) {
    	m_barrierPendingCnt = m_numChildren + m_ranksPer;
	    if ( m_barrierRoot ) {
		    barrierFanout( );
	    } else {	
		    sendBarrierMsg( m_localNidMap[0], OrteMsg::u::barrier::Up );
	    }
    }
}

void Orte::processBarrierMsg( OrteMsg* msg )
{
	Debug( Orte, "enter\n");
	if ( msg->u.barrier.type == OrteMsg::u::barrier::Up ) {
		barrier( NULL );
	} else {
		barrierFanout( );
	}
}

void Orte::barrierFanout()
{
    Debug( Orte, "enter\n");
    for ( size_t i = 1; i < m_localNidMap.size(); i++ ) {
	    sendBarrierMsg( m_localNidMap[i], OrteMsg::u::barrier::Down ); 
    }

    LwkOrteRmlMsg msg;
    msg.type = LwkOrteRmlMsg::BARRIER;

    std::map< int, std::pair<int,int> >::iterator   iter = m_pid2fdM.begin();
    for ( ; iter != m_pid2fdM.end(); ++iter ) {
        int fd = iter->second.second;
        Debug( Orte, "write() fd=%d \n", fd );
pthread_mutex_lock(&m_mutex);
        if ( write( fd, &msg, sizeof( msg ) ) != sizeof( msg ) ) {
            Warn( Orte, "write() fd=%d \n", fd );
            abort();
        }	 
pthread_mutex_unlock(&m_mutex);
    }
}

void Orte::sendBarrierMsg( ProcId& dest, OrteMsg::u::barrier::type_t type )
{
    OrteMsg*  msg = new OrteMsg;

    Debug( Orte, "[%#x,%d]\n", dest.nid, dest.pid );

    msg->type = OrteMsg::Barrier;
    msg->u.barrier.type = type;

    sendMsg( msg, dest );
}

void Orte::sendOOBMsg( LwkOrteRmlMsg* hdr, unsigned char * body )
{
    Debug( Orte, "[%d,%d,%d] tag=%#x nbytes=%d\n",
		hdr->u.oob.peer.cellid, hdr->u.oob.peer.jobid, 
		hdr->u.oob.peer.vpid, hdr->u.oob.tag, hdr->u.oob.nBytes );

#if 0
     m_log.write( "%s:%d: rank=%d\n",__func__,__LINE__,
                                hdr->u.oob.peer.vpid ); 
#endif

    if ( hdr->u.oob.peer.vpid >= m_baseRank && 
                hdr->u.oob.peer.vpid < m_baseRank + m_ranksPer ) {
	    // should this be non-blocking?
        Debug( Orte, "local\n" );
        int fd = m_pid2fdM[ m_rank2PidM[hdr->u.oob.peer.vpid] ].second;
pthread_mutex_lock(&m_mutex);
	    if ( (size_t) write( fd, hdr, sizeof(*hdr)  ) != sizeof(*hdr) ) {
            Debug( Orte, " write failed\n" );
            exit(-1);
	    }
	    if ( write( fd, body, hdr->u.oob.nBytes  ) != hdr->u.oob.nBytes ) {
            Debug( Orte, " write failed\n" );
            exit(-1);
	    }
pthread_mutex_unlock(&m_mutex);
        return;
    }

    ProcId dest; 
    OrteMsg*  msg = new OrteMsg;

    msg->type = OrteMsg::OOB;

    dest.nid = m_route.rank2nid( hdr->u.oob.peer.vpid );
    dest.pid = m_dm.pid();

    Debug( Orte, "[%#x,%d]\n", dest.nid, dest.pid );

    memcpy( msg->u.oob.data, hdr, sizeof(*hdr) );
    memcpy( msg->u.oob.data + sizeof(*hdr), body, hdr->u.oob.nBytes );
    delete hdr;
    delete[] body;

    sendMsg( msg, dest );
}

void Orte::sendMsg( OrteMsg* msg, ProcId& dest)
{
    Request* req = new Request( sendCallback, (void*) this );

    std::pair<Request*,void*>* tmp = new std::pair<Request*,void*>( req, msg );
    req->setCbData( tmp );

    if( m_dm.isend( msg, sizeof(*msg), dest, ORTE_MSG_TAG, *req ) ) {
        printf("isend failed\n");
        exit(-1);
    }
}

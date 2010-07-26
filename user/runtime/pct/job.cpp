#include <pct/util.h>
#include <pct/job.h>
#include <pct/app.h>
#include <sys/types.h>
#include <sys/wait.h>

Job::JobMap Job::m_jobMap;
pthread_mutex_t Job::m_mutex = PTHREAD_MUTEX_INITIALIZER;

Job::Job( Rdma& rdma, JobId jobId ) :
    m_rdma( rdma ),
    m_jobId( jobId ),
    m_numChildStarted( 0 ),
    m_numChildAlive( 0 ),
    m_numLocalAlive( 0 ),
    m_havePidPart( 0 ),
    m_nChildRanks( 0 ),
    m_nChildNids( 0 ),
    m_stride( 0 )
{
    Info( "starting new job %d\n", jobId);
}

Job::~Job( )
{
    Debug( Job, "enter\n");
    Info( "job %d has terminated\n", m_jobId );

    delete m_app;

    Debug( Job, "\n");

	// note we only disconnect our children
    for ( unsigned int i = 1; i < m_localNidMap.size(); i++ ) {
	Debug( Job, "disconnect nid=%#x\n", m_localNidMap[i].nid );
        m_rdma.disconnect( m_localNidMap[i] );
    }

    m_rdma.memRegionUnregister( NidRankId );
    m_rdma.memRegionUnregister( NidPidId );
    Debug( Job, "return\n");
}

void* Job::thread( void* obj )
{
    Job* job = (Job*) obj;
    Debug(Job,"entered\n");

    if ( pthread_detach( pthread_self() ) ) {
        Warn( Job, "ptherad_detach()\n" );
        throw -1;
    }

    bool alive = true;
    while ( alive ) {
        int status;

	//printf("call waitpid()\n");
        pid_t pid = ::waitpid( -1, &status, 0 );

        Info( "process %d has exited with status %d\n", pid, status );

        pthread_mutex_lock(&m_mutex);

        --job->m_numLocalAlive;
        if ( job->m_numLocalAlive == 0 ) {
            if ( job->m_numChildAlive == 0 ) {
                job->sendChildExit();
                m_jobMap.erase( m_jobMap.begin() );
                delete job;
            }
            alive = false;        
        }
        pthread_mutex_unlock(&m_mutex);
    }
    Debug(Job,"returning\n");
    return NULL;
}
                        
void Job::msg( Rdma& rdma, ProcId src, JobMsg& msg )
{
    JobMap::iterator iter;

    Debug( Job, "jobId=%d type=%d\n", msg.jobId, msg.type );

    pthread_mutex_lock(&m_mutex);

    if ( msg.type == JobMsg::Load ) {

        if ( m_jobMap.find( msg.jobId ) != m_jobMap.end() ) {
            // take care of this error
            Warn( Job, "duplicate job %d\n", msg.jobId );
            goto ret;
        }

        Job* job = new Job( rdma, msg.jobId ); 
        if ( ! job ) {
            Warn( Job, "couldn't create Job %d\n", msg.jobId  );
            goto ret;
        }
        iter = m_jobMap.insert( m_jobMap.begin(), 
                            std::make_pair( msg.jobId, job ) );

    } else {
        if ( ( iter = m_jobMap.find( msg.jobId ) ) == m_jobMap.end() ) {
            // take care of this error
            Warn( Job, "couldn't find Job %d\n", msg.jobId );
            goto ret;
        }
    }

    if ( iter->second->msg( src, msg ) ) {
        delete iter->second;
        m_jobMap.erase( iter );
    }
   
ret:
    pthread_mutex_unlock(&m_mutex);
}

bool Job::msg( ProcId src, JobMsg& msg )
{
    Debug( Job, "type=%d\n", msg.type );

    if ( m_jobId != msg.jobId ) return true;
    switch( msg.type ) {

        case JobMsg::Load:
            load( src, msg.u.load );
            break;

        case JobMsg::Kill:
            kill( msg.u.kill );
            break;

        case JobMsg::ChildExit:
            return childExitMsg(  msg.u.childExit );

        case JobMsg::ChildStart:
            childStart( msg.u.childStart );
            break;

        case JobMsg::NidPidMapPart:
            nidPidMapPart( src, msg.u.nidPidMapPart );
            break;

        case JobMsg::NidPidMapAll:
            nidPidMapAll( src, msg.u.nidPidMapAll );
            break;

        default:
            Warn( Job, "msg.jobId=%d unknown type=%d\n", msg.jobId, msg.type);
            throw -1;
    }        
    return false;
}

void Job::start()
{
    Debug( Job, "\n");

    sendChildStart( );

    m_app->Start(); 

    if ( pthread_create( &m_thread, NULL, thread, this ) ) {
        Warn( Job, "pthread_create() failed, %s\n", strerror(errno) );
        throw -1;
    }

}

bool Job::kill( struct JobMsg::Kill& msg )
{
    Debug( Job, "\n");

    m_app->Kill( msg.signal );

    if ( m_nChildNids ) {
        fanoutKill( msg.signal );
    } 

    return false;
}

bool Job::load( ProcId src, struct JobMsg::Load& msg )
{
    m_nidRnkMap.resize( msg.nNids );
    m_childNum = msg.childNum;

    Debug( Job, "got LOAD msg jobId=%d childNum=%d\n", m_jobId, m_childNum );
    Debug( Job, "nRanks=%d nNids=%d fanout=%d\n", 
                                msg.app.nRanks, msg.nNids, msg.fanout );

    m_rdma.memRegionRegister( m_nidRnkMap.data(), sizeofVec(m_nidRnkMap),
                            NidRankId );

    // read the nid/ranksPer/baseRank map
    Debug(Job,"read nid/rankPer\n");
    if ( m_rdma.memRead( src, msg.nidRnkMapKey, msg.nidRnkMapAddr,
                    m_nidRnkMap.data(), 
                    sizeofVec( m_nidRnkMap ) ))
    {
        Warn( Job, "nidMap read failed\n" );
        return true;
    }

    uint nSubNids = msg.nNids - 1; 

    if ( nSubNids ) {
        m_stride = nSubNids / msg.fanout + ( nSubNids % msg.fanout ? 1 : 0 ); 
        m_nChildNids = nSubNids / m_stride + ( nSubNids % m_stride ? 1 : 0 );
    }

    Debug( Job, "nChildNids=%d stride=%d\n", m_nChildNids, m_stride );

    m_route.add( src.nid );

    m_localNidMap.resize( m_nChildNids + 1 );
    m_childStartedV.resize( m_nChildNids );

    m_numChildAlive = m_nChildNids;
    m_numLocalAlive =  nRanksThisNode();
    // set the parent info
    m_localNidMap[0] = src; 

    for ( unsigned int child = 0; child < m_nChildNids; child++ )
    {
        int offset = child * m_stride + 1;

        Debug( Job, "offset=%d\n", offset );
        m_localNidMap[ child + 1 ].nid = m_nidRnkMap[ offset ].nid;
        m_localNidMap[ child + 1 ].pid = m_rdma.pid();

        Debug( Job, "child=%d nid=%#x pid=%d\n", child, 
                        m_localNidMap[ child + 1 ].nid,
                        m_localNidMap[ child + 1 ].pid);

        m_rdma.connect( m_localNidMap[child+1] );

    }

    m_app = new App( m_rdma, m_route, src, msg.app, m_nidRnkMap[0],
                                                        m_localNidMap );

    m_rdma.memRegionRegister( &m_app->NidPidMap(0), 
                            m_app->NidPidMapSize() * sizeof(ProcId),
                            NidPidId );

    if ( m_nChildNids == 0 ) {
        sendNidPidMapPart();
    } else {
        fanoutLoadMsg( msg, m_nidRnkMap[0] );
    }
    return false;
}


bool Job::childStart( struct JobMsg::ChildStart& msg )
{
    Debug( Job, "child %d\n", msg.num);

    m_childStartedV[msg.num] = true;

    ++m_numChildStarted;

    if ( m_numChildStarted == m_nChildNids ) {
        start();
    }
    return 0;
}

bool Job::childExitMsg( struct JobMsg::ChildExit& msg )
{
    Debug( Job, "child %d\n", msg.num);
    m_childStartedV[msg.num] = false;
    --m_numChildAlive;

    if ( m_numChildAlive == 0 && m_numLocalAlive == 0 ) {
        sendChildExit();
        return true;
    }
    return false;
}

bool Job::sendCtrlMsg2Child( CtrlMsg& msg, int toChild )
{
    Debug( Job, "toChild=%d\n",toChild );
    return sendCtrlMsg( msg, m_localNidMap[ toChild + 1 ] );
}

bool Job::sendCtrlMsg2Parent( CtrlMsg& msg, int fromChild )
{
    Debug( Job, "fromChild=%d\n", fromChild );
    return sendCtrlMsg( msg, m_localNidMap[ 0 ] );
}

bool Job::sendCtrlMsg( CtrlMsg& msg, ProcId& dest )
{
    msg.type = CtrlMsg::Job;
    msg.src.nid = m_rdma.nid();
    msg.src.pid = m_rdma.pid();
    msg.u.job.jobId = m_jobId;

    Debug( Job, "type=%d nid=%#x pid=%d\n", msg.u.job.type, dest.nid, dest.pid);

#if 1 
    Status status;
    m_rdma.send( &msg, sizeof(msg), dest, CTRL_MSG_TAG, &status );
#else

    Request* req = new Request( sendCallback, (void*) this );
    
    CtrlMsg* _msg = new CtrlMsg;
	memcpy(_msg,&msg,sizeof(msg));
    std::pair<Request*,void*>* tmp = new std::pair<Request*,void*>( req, _msg );
    req->setCbData( tmp );
    
    m_rdma.isend( _msg, sizeof(*_msg), dest, CTRL_MSG_TAG, *req );
#endif

    return false;
}

void* Job::sendCallback( void* obj, void *data )
{
    return ( ( Job*)obj)->sendCallback( (sendCbData_t*) data );
}

void* Job::sendCallback( sendCbData_t* data  )
{
        Debug( Job, "enter\n");

        // Request*
        delete data->first;

        // CtrlMsg*
        delete data->second;

        //std::pair<Request*,void*>
        delete data;
        return NULL;
}


bool Job::fanoutLoadMsg( struct JobMsg::Load& _msg, NidRnk& parentNidRnk )
{
    Debug( Job, "%d\n",m_nChildNids);

    CtrlMsg msg;
    msg.u.job.type = JobMsg::Load;

    struct JobMsg::Load& loadMsg = msg.u.job.u.load;

    loadMsg = _msg;

    loadMsg.app.imageKey = App::ImageId;
    loadMsg.app.imageAddr = 0;

    loadMsg.nidRnkMapKey = NidRankId;

    loadMsg.nNids = m_stride;

    for ( unsigned int child = 0; child < m_nChildNids; child++ ) {

        if ( ( child + 1 ) * m_stride > _msg.nNids - 1 ) {
            loadMsg.nNids = (_msg.nNids - 1) % m_stride ;
        }

        int offset = child * m_stride + 1;
        loadMsg.childNum = child;
        loadMsg.nidRnkMapAddr = offset * sizeof( struct NidRnk );

        Debug( Job, "child=%d addr=%#lx nNids=%d\n", 
                            child, loadMsg.nidRnkMapAddr, loadMsg.nNids);

        if ( sendCtrlMsg2Child( msg, child ) ) {
            return true;
        }
    }   
    Debug( Job, "return\n");
    return false;
}


bool Job::nidPidMapPart( ProcId src, struct JobMsg::NidPidMapPart& msg )
{
    ++m_havePidPart;

    Debug( Job, "num=%d base=%d m_havePidPart=%d\n", 
			msg.numRank, msg.baseRank, m_havePidPart );

    m_route.add( src.nid, msg.baseRank, msg.baseRank + msg.numRank );
    if ( m_rdma.memRead( src, msg.key, msg.addr,
                    &m_app->NidPidMap( msg.baseRank ),
                    msg.numRank * sizeof( ProcId ) ))
    {
    	Debug( Job, "memRead Failed\n");
        return true;        
    }
    Debug( Job, "\n");

    m_nChildRanks += msg.numRank;

    if ( m_havePidPart == m_nChildNids ) {
    Debug( Job, "\n");
        sendNidPidMapPart();
    } 
    Debug( Job, "\n");

    return false;
}

bool Job::nidPidMapAll( ProcId src, struct JobMsg::NidPidMapAll& msg )
{
    Debug( Job, "src=[%#x,%d]\n",src.nid,src.pid);

    if ( m_rdma.memRead( src, msg.key, msg.addr,
                    &m_app->NidPidMap( 0 ),
                    m_app->NidPidMapSize() * sizeof( ProcId ) ) ) 
    {
        return true;        
    }

    for ( unsigned int i = 0; i < m_app->NidPidMapSize(); i++ ) {
        Debug( Job, "rank=%d nid=%#x pid=%d\n",i,
                m_app->NidPidMap(i).nid, m_app->NidPidMap(i).pid);
    }

    if ( m_nChildNids == 0 ) {
        start();
    } else { 
        fanoutPidMap();
    }
    return false;
}

void Job::fanoutPidMap( )
{
    Debug( Job, "\n");

    CtrlMsg msg;

    msg.u.job.type = JobMsg::NidPidMapAll;

    msg.u.job.u.nidPidMapAll.key = NidPidId;
    msg.u.job.u.nidPidMapAll.addr = 0;

    for ( unsigned int child = 0; child < m_nChildNids; child++ ) {
        sendCtrlMsg2Child( msg, child );
    }
}

void Job::fanoutKill( int signal )
{
    Debug( Job, "\n");

    CtrlMsg msg;

    msg.u.job.type = JobMsg::Kill;

    msg.u.job.u.kill.signal = signal;

    for ( unsigned int child = 0; child < m_nChildNids; child++ ) {
        if ( m_childStartedV[child] ) {
            sendCtrlMsg2Child( msg, child );
        }
    }
}

void Job::sendChildStart(  )
{
    Debug( Job, "\n");

    CtrlMsg msg;

    msg.u.job.type = JobMsg::ChildStart;
    msg.u.job.u.childStart.num = m_childNum;

    sendCtrlMsg2Parent( msg, m_childNum );
}

int Job::sendNidPidMapPart( )
{
    Debug( Job, "\n");

    CtrlMsg msg;

    msg.u.job.type = JobMsg::NidPidMapPart;

    msg.u.job.u.nidPidMapPart.key = NidPidId;
    msg.u.job.u.nidPidMapPart.addr = baseRank() * sizeof(ProcId);
    msg.u.job.u.nidPidMapPart.baseRank = baseRank();
    msg.u.job.u.nidPidMapPart.numRank = m_nChildRanks + nRanksThisNode();

    sendCtrlMsg2Parent( msg, m_childNum );
    return 0; 
}

int Job::sendChildExit( )
{
    Debug( Job, "\n");

    CtrlMsg msg;

    msg.u.job.type = JobMsg::ChildExit;
    msg.u.job.u.childStart.num = m_childNum;

    sendCtrlMsg2Parent( msg, m_childNum );

    return 0; 
}

// do we really need this
uint Job::nRanksThisNode()
{
    return m_nidRnkMap[0].ranksPer;
}

// do we really need this
uint Job::baseRank()
{
    return m_nidRnkMap[0].baseRank;
}

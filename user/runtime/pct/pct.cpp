#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>

#include <pct/pct.h>
#include <pct/app.h>
#include <pct/job.h>
#include <pct/mem.h>
#include <pct/util.h>

#include <sched.h>

#include <pct/debug.h>
//#include <pct/fileSystem.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

Pct::Pct()
{
	const char* tmp = "/etc/opal/fifo";
	m_ompiFd =  open( tmp, O_RDONLY );
	if ( m_ompiFd == -1 ) {
                printf("couldn't open `%s`\n",tmp);
                abort();
	}
}

Pct::~Pct()
{
    //delete m_fileSystem;
}

int Pct::Go()
{
#if 0
    try {
        RDMA::nodeId_t server;// = { }
        m_fileSystem = new FileSystem( m_rdma, server, "/lwk" );
    } catch (int err ){
        Warn( Pct, "new FileSystem() failed error %d\n", err );
        return 0;  
    }
#endif

    bool rearm = true;
    Request req;	
while(1) {
    CtrlMsg msg;
    Status  status;
    ProcId  id = { -1, -1 };	

    if ( rearm ) {
        if ( m_rdma.irecv( &msg, sizeof(msg), id, CTRL_MSG_TAG, req ) > 0 ) {
    	    Debug(Pct,"irecv failed\n");
            exit(-1);
        }
        rearm = false;	
    }

    if ( m_rdma.wait( req, 0, &status ) == 1 ) {
        ctrlMsgProcess( msg );
    	rearm = true;	
    }

#if 0
    struct pollfd fds[1];
    fds[0].fd = m_ompiFd;
    fds[0].events = POLLIN;
    if ( poll( fds, 1, 0 ) > 0 ) {
        Debug(Pct,"MPI something to read\n");
    }
#endif
    
    JobMap::iterator iter;

    for ( iter = m_jobMap.begin(); iter != m_jobMap.end(); ++iter ) { 
        if ( iter->second->Process() ) {
            Debug( Pct, "Process() returned true\n" );
            try {
                delete iter->second;
            } catch ( int err ) {
                printf("delete Job() failed %d\n",err);
                abort();
            }
            m_jobMap.erase( iter );
        }
    }

    //m_fileSystem->Work();
}
    return 0;
}

void Pct::jobMsg( ProcId& srcNode, struct JobMsg& msg )
{
    JobMap::iterator iter;

    Debug( Pct, "jobId=%d type=%d\n", msg.jobId, msg.type );

    if ( msg.type == JobMsg::Load ) {

        if ( m_jobMap.find( msg.jobId ) != m_jobMap.end() ) {
            // take care of this error
            Warn( Pct, "duplicate job %d\n", msg.jobId );
            return; 
        }

        Job* job = new Job( m_rdma, msg.jobId ); 
        if ( ! job ) {
            Warn( Pct, "couldn't create Job %d\n", msg.jobId  );
            return;
        }
        iter = m_jobMap.insert( m_jobMap.begin(), 
                            std::make_pair( msg.jobId, job ) );

    } else {
        if ( ( iter = m_jobMap.find( msg.jobId ) ) == m_jobMap.end() ) {
            // take care of this error
            Warn( Pct, "couldn't find Job %d\n", msg.jobId );
            return; 
        }
    }

    if ( iter->second->Msg( srcNode, msg ) ) {
        Warn( Pct, "Job::Msg() failed jobid=%d\n", msg.jobId );
        // take care of error
    }
}

void Pct::ctrlMsgProcess( CtrlMsg& msg )
{
    Debug( Pct, "got ctrl msg %d from %#x:%d\n",
                    msg.u.job.type, msg.src.nid, msg.src.pid );

    switch ( msg.type ) {
        case CtrlMsg::Job:
            jobMsg( msg.src, msg.u.job ); 
            break;
    }
}

#include <pct/pct.h>
#include <pct/job.h>
#include <pct/debug.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

Pct::Pct() 
{
    Debug(Pct,"\n");

    if ( mkdir("/tmp",0777) ) {
        Warn(,"mkdir( `/tmp`, 0777) failed\n");
        throw -1;
    }

    if ( mkdir("/proc",0777) ) {
        Warn(,"mkdir( `/etc`, 0777) failed\n");
        throw -1;
    }

}

Pct::~Pct()
{
#if 0
    delete m_fileSystem;
#endif
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

    while(1) {
        CtrlMsg msg;
        Status  status;
        ProcId  id = { -1, -1 };	

        Debug( Pct, "waiting for CTRL_MSG\n");
	//printf("calling m_rdma.recv()\n");
        if ( m_rdma.recv( &msg, sizeof(msg), id, CTRL_MSG_TAG, &status ) != 1 )
        {
            Warn(Pct,"recv failed\n");
            throw -1;
        }
        ctrlMsgProcess( msg );
    }
    return 0;
}

void Pct::ctrlMsgProcess( CtrlMsg& msg )
{
    Debug( Pct, "got ctrl msg %d from %#x:%d\n",
                    msg.u.job.type, msg.src.nid, msg.src.pid );

    switch ( msg.type ) {
        case CtrlMsg::Job:
            Job::msg( m_rdma, msg.src, msg.u.job ); 
            break;
    }
}

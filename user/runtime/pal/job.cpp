#include <pal/job.h>
#include <pal/config.h>
#include <pal/debug.h>
#include <pal/util.h>

Job::Job( Dm& rdma, Config& config ) :
    m_config( config ),
    m_rdma( rdma ),
    m_kill(false)
{
    m_myNodeId.nid = m_rdma.nid();
    m_myNodeId.pid = m_rdma.pid();
    m_rootNodeId.nid = config.baseNid();
    m_rootNodeId.pid = config.pctPid();

    m_nidPidMap.resize( config.nRanks() );

    m_rdma.memRegionRegister( config.nidRnkMap().data(), sizeofVec( config.nidRnkMap() ),
                                    NidRankId );

    m_rdma.memRegionRegister( config.appImage().data(), config.appImageLen(),
                                    ImageId );

    m_rdma.memRegionRegister( m_nidPidMap.data(), sizeofVec(m_nidPidMap),
                                    NidPidId );

    m_jobId = config.jobId();
}

Job::~Job()
{
    m_rdma.memRegionUnregister( NidRankId );
    m_rdma.memRegionUnregister( ImageId );
    m_rdma.memRegionUnregister( NidPidId );
}

bool Job::Kill( int sig )
{
    if ( m_kill ) {
        Debug(Pal,"hold on\n");
        return true;
    }
    m_kill = true;

    CtrlMsg msg;

    msg.u.job.type = JobMsg::Kill;

    sendCtrlMsg( msg );
    return false;
}


bool Job::Load()
{
    CtrlMsg msg;

    msg.u.job.type                 = JobMsg::Load;
    msg.u.job.u.load.imageKey      = ImageId;
    msg.u.job.u.load.imageAddr     = 0;
    msg.u.job.u.load.elf_len       = m_config.elfLen(); 
    msg.u.job.u.load.cmdLine_len   = m_config.cmdLen();
    msg.u.job.u.load.env_len       = m_config.envLen();

    msg.u.job.u.load.heap_len      = m_config.heapLen();
    msg.u.job.u.load.stack_len     = m_config.stackLen();

    msg.u.job.u.load.nNids         = m_config.nNids(); 
    msg.u.job.u.load.fanout        = m_config.fanout();
    msg.u.job.u.load.nRanks        = m_config.nRanks();
    msg.u.job.u.load.childNum      = 0;

    msg.u.job.u.load.nidRnkMapKey  = NidRankId;
    msg.u.job.u.load.nidRnkMapAddr = 0;

    sendCtrlMsg( msg );
    return false;
}

bool Job::havePidPart( struct JobMsg::NidPidMapPart& msg )
{
    Debug( Pal, "\n" );
    if ( m_rdma.memRead( m_rootNodeId, msg.key, msg.addr,
                    &m_nidPidMap.at( msg.baseRank ),
                    msg.numRank * sizeof( ProcId ) ) ) 
    {

    }

    for ( unsigned int i = 0; i < msg.numRank; i++ ) {
        Debug( Pal, "rank=%d nid=%#x pid=%d\n", i, 
                        m_nidPidMap[i].nid,  m_nidPidMap[i].pid );
    }

    CtrlMsg msg2;

    msg2.u.job.type                 = JobMsg::NidPidMapAll;
    msg2.u.job.u.nidPidMapAll.key   = NidPidId;
    msg2.u.job.u.nidPidMapAll.addr  = 0;

    sendCtrlMsg( msg2 );
    return false;
}

bool Job::childStart()
{
    return false;
}

bool Job::sendCtrlMsg( CtrlMsg& msg )
{
    msg.type        = CtrlMsg::Job;
    msg.src     = m_myNodeId;
    msg.u.job.jobId = m_jobId; 

    Debug( Pal, "type=%d\n", msg.u.job.type );
    Status status;	
    m_rdma.send( &msg, sizeof(msg), m_rootNodeId, CTRL_MSG_TAG, &status);
    return false;
}

bool Job::JobMsg( ProcId& srcNode, struct JobMsg& msg )
{
    Debug( Job, "jobId=%d type=%d\n", m_jobId, msg.type );

    if ( m_jobId != msg.jobId ) return true;
    // should we verify the srcNode

    switch ( msg.type ) {
        case JobMsg::NidPidMapPart:
            havePidPart( msg.u.nidPidMapPart );
		Debug(Job,"\n");
            break; 

        case JobMsg::ChildExit:
            Debug( Job, "Child exited\n");
            return true;

        case JobMsg::ChildStart:
            Debug( Job, "Child started\n");
            break;

        default:
            break;
    }
    return false;
}

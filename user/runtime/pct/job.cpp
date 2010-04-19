
#include <stdlib.h>
#include <assert.h>

#include <pct/util.h>
#include <pct/job.h>
#include <pct/app.h>
#include <pct/debug.h>
#include <pct/msgs.h>

Job::Job( Dm& rdma, JobId jobId ) :
    m_rdma( rdma ),
    m_jobId( jobId ),
    m_numStarted( 0 ),
    m_numExited( 0 ),
    m_havePidPart( 0 ),
    m_nChildRanks( 0 ),
    m_stride( 0 )
{
}

Job::~Job( )
{
    Debug( Job, "enter\n");
    try {
        delete m_app;
    } catch ( int err ) {
        printf("delete App() failed %d\n",err);
        throw -1;
    }

	// note we only disconnect our children
    for ( unsigned int i = 1; i < m_localNidMap.size(); i++ ) {
        m_rdma.disconnect( m_localNidMap[i] );
    }

    m_rdma.memRegionUnregister( ImageId );
    m_rdma.memRegionUnregister( NidRankId );
    m_rdma.memRegionUnregister( NidPidId );
}

ProcId& Job::parentNode()
{
    return m_localNidMap[0];
}

bool Job::Msg( ProcId src, JobMsg& msg )
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
            childExit( msg.u.childExit );
            break;

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
            return true;
    }        
    return false;
}

void Job::start()
{
    Debug( Job, "\n");

    m_app->Start(); 
    sendChildStart( );
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
    m_localNidMap.resize( 1 );
    m_localNidMap[0] = src; 
    m_childNum = msg.childNum;

    Debug( Pct, "got LOAD msg jobId=%d childNum=%d\n", m_jobId, m_childNum );

    try {
        m_app = new App( msg.nRanks, msg.elf_len, msg.heap_len, msg.stack_len,
                        msg.cmdLine_len, msg.env_len, msg.uid, msg.gid );
        assert(m_app);
    } catch( int err ) {
        Warn( Job, "new App() failed %d\n",err);
        return true;
    }

    m_rdma.memRegionRegister( m_app->ImageAddr(), m_app->ImageLen(),
				ImageId );
    m_rdma.memRegionRegister( m_nidRnkMap.data(), sizeofVec( m_nidRnkMap),
                            NidRankId );
    m_rdma.memRegionRegister( &m_app->NidPidMap(0), 
                            m_app->NidPidMapSize() * sizeof(ProcId),
                            NidPidId );

    Debug( Job, "nRanks=%d nNids=%d fanout=%d\n", 
                                msg.nRanks, msg.nNids, msg.fanout );

    // read the elfimage + cmdline + env
Debug(Pct,"read image\n");
    if ( m_rdma.memRead( parentNode(), msg.imageKey, msg.imageAddr,
                    m_app->ImageAddr(), m_app->ImageLen() ) ) 
    {
        Warn( Job, "image read failed\n" );
        return true;
    } 

    // read the nid/ranksPer/baseRank map
Debug(Pct,"read nid/rankPer\n");
    if ( m_rdma.memRead( parentNode(), msg.nidRnkMapKey, msg.nidRnkMapAddr,
                    m_nidRnkMap.data(), 
                    sizeofVec( m_nidRnkMap ) ))
    {
        Warn( Job, "nidMap read failed\n" );
        return true;
    }

    for ( m_nidIndex = 0; m_nidIndex < msg.nNids; m_nidIndex++ ) {
        if ( m_nidRnkMap[m_nidIndex].nid == m_rdma.nid() ) {
            break;
        }
    }

    if ( m_nidIndex == msg.nNids ) {
        Warn( Job, "bogus nidIndex %d %d\n", m_nidIndex, msg.nNids );
        return true;
    }

    Debug( Job, "baseRank=%d ranksPer=%d\n", baseRank(), nRanksThisNode() );

    m_app->AllocPids( m_rdma.nid(), baseRank(), nRanksThisNode() );

    uint nSubNids = msg.nNids - 1; 

    m_nChildNids = msg.fanout > nSubNids ? nSubNids : msg.fanout;

    if ( m_nChildNids ) {
        m_stride = nSubNids / m_nChildNids + 
                                ( ( nSubNids % m_nChildNids ) ? 1 : 0);  
    }

    Debug( Job, "nChildNids=%d stride=%d\n", m_nChildNids, m_stride );

    m_localNidMap.resize( m_nChildNids + 1 );

    for ( unsigned int child = 0; child < m_nChildNids; child++ )
    {
        m_localNidMap[ child + 1 ].nid = 
                                m_nidRnkMap[ child * m_stride + 1 ].nid;
        m_localNidMap[ child + 1 ].pid = m_rdma.pid();
        Debug( Job, "child=%d nid=%d pid=%d\n", child, 
                        m_localNidMap[ child + 1 ].nid,
                        m_localNidMap[ child + 1 ].pid);
    }

    if ( m_nChildNids == 0 ) {
        sendNidPidMapPart();
    } else {
        fanoutLoadMsg( msg );
    }
    return false;
}


bool Job::childStart( struct JobMsg::ChildStart& msg )
{
    Debug( Job, "child %d\n", msg.num);

//    sendGotStart( msg.num );

    ++m_numStarted;

    if ( m_numStarted == m_nChildNids ) {
        start();
    }
    return 0;
}


bool Job::childExit( struct JobMsg::ChildExit& msg )
{
    Debug( Job, "child %d\n", msg.num);
    ++m_numExited;
    return false;
}

bool Job::Process()
{
    if ( m_app->Exited() )
    { 
        if ( m_nChildNids == 0 || 
		( m_nChildNids && m_numExited == m_nChildNids ) ) 
        {
            Debug( Job, "app has exited\n");
            sendChildExit();
            return true;
        }
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

    Status status;
    m_rdma.send( &msg, sizeof(msg), dest, CTRL_MSG_TAG, &status );

    return false;
}

bool Job::fanoutLoadMsg( struct JobMsg::Load& _msg )
{
    Debug( Job, "%d\n",m_nChildNids);

    CtrlMsg msg;
    msg.u.job.type = JobMsg::Load;

    struct JobMsg::Load& loadMsg = msg.u.job.u.load;

    loadMsg = _msg;

    loadMsg.imageKey = ImageId;
    loadMsg.imageAddr = 0;

    loadMsg.nidRnkMapKey = NidRankId;

    loadMsg.nNids = m_stride;

    for ( unsigned int child = 0; child < m_nChildNids; child++ ) {

        if ( ( child + 1 ) * m_stride > _msg.nNids - 1 ) {
            loadMsg.nNids = (_msg.nNids - 1) % m_stride ;
        }

        loadMsg.nidRnkMapAddr = 
                            (child * m_stride + 1) * sizeof( struct NidRnk );
        loadMsg.childNum = child;

        Debug( Job, "child=%d addr=%#lx nNids=%d\n", 
                            child, loadMsg.nidRnkMapAddr, loadMsg.nNids );

        if ( sendCtrlMsg2Child( msg, child ) ) {
            return true;
        }
    }   
    return false;
}


bool Job::nidPidMapPart( ProcId src, struct JobMsg::NidPidMapPart& msg )
{
    ++m_havePidPart;

    Debug( Job, "num=%d base=%d m_havePidPart=%d\n", 
			msg.numRank, msg.baseRank, m_havePidPart );

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
    Debug( Job, "\n");

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
        sendCtrlMsg2Child( msg, child );
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

uint Job::nRanksThisNode()
{
    return m_nidRnkMap[m_nidIndex].ranksPer;
}

uint Job::baseRank()
{
    return m_nidRnkMap[m_nidIndex].baseRank;
}

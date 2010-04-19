#include <pal/pal.h>
#include <pal/job.h>
#include <pal/config.h>
#include <pal/debug.h>
#include <pal/util.h>
#include <stdlib.h>

Pal::Pal( )
{
}

Pal::~Pal()
{
}

int Pal::Run( Config& config )
{
    m_job = new Job( m_rdma, config );
    m_job->Load( );

    while(1) 
    {
	CtrlMsg msg;
	Status status;
	ProcId id = { -1, -1 };
        if ( m_rdma.recv( &msg, sizeof(msg), id, CTRL_MSG_TAG, &status ) > 0 ) {
            Debug( App, "do_ctrl\n");
            if ( do_ctrl( msg ) ) {
                break;
            }
        }
    }

    delete m_job;
    return 0;
}

bool Pal::do_ctrl( CtrlMsg& msg) 
{
    Debug( Pal, "got ctrl msg %d from %#x:%d\n",msg.u.job.type,
                            msg.src.nid, msg.src.pid );

    switch ( msg.type ) {
        case CtrlMsg::Job:
            return m_job->JobMsg( msg.src, msg.u.job );
            break;

        default:
            break;
    }
    return false;
}

void Pal::Sighandler( int sig ) 
{
    Debug(Pal,"\n");    

    if ( m_job->Kill( sig ) ) {
        delete m_job;
        exit(0);
    }
}

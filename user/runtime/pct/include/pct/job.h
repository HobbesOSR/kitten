
#ifndef JOB_APP_H
#define JOB_APP_H

#include <sys/types.h>
#include <pct/msgs.h>
#include <vector>

using namespace srdma;

class App;

class Job {
	static const int ImageId = 0xf00d0001;
	static const int NidRankId = 0xf00d0002;
	static const int NidPidId = 0xf00d0003;
	
    public:
        Job( srdma::Dm& rdma, JobId jobId );
        ~Job();
        bool Msg( ProcId src, JobMsg& msg );
        bool Process();

    private:
        Job( const Job& );

        bool load( ProcId src, struct JobMsg::Load& );
        bool kill( struct JobMsg::Kill& );
        bool nidPidMapAll( ProcId src, struct JobMsg::NidPidMapAll& );

        bool childExit( struct JobMsg::ChildExit& );
        bool childStart( struct JobMsg::ChildStart& );
        bool nidPidMapPart( ProcId src, struct JobMsg::NidPidMapPart& );
        bool gotStart();

        int sendChildExit();
        int sendNidPidMapPart();
        void sendChildStart( );
        bool sendGotStart( uint child );
        bool fanoutLoadMsg( struct JobMsg::Load& );
        void fanoutPidMap();
        void fanoutKill( int signal );

        uint nRanksThisNode(); 
        uint baseRank();

        void start();
        bool sendCtrlMsg2Child( CtrlMsg&, int fromChild );
        bool sendCtrlMsg2Parent( CtrlMsg&, int toChild );
        bool sendCtrlMsg( CtrlMsg&, ProcId& dest );

        ProcId& parentNode( );

        Dm&          m_rdma;
        JobId               m_jobId;

        App*                m_app;
        std::vector<NidRnk> m_nidRnkMap;
        unsigned int        m_nidIndex;

        unsigned int        m_numStarted;
        unsigned int        m_numExited;
        uint                m_havePidPart;
        uint                m_nChildRanks;
        uint                m_nChildNids;
        uint                m_stride;
//        bool                m_gotStart;
        uint                m_childNum;

        std::vector< ProcId>    m_localNidMap;
};

#endif

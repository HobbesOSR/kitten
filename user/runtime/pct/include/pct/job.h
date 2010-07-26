
#ifndef JOB_APP_H
#define JOB_APP_H

#include <sys/types.h>
#include <pct/msgs.h>
#include <pct/route.h>
#include <vector>
#include <signal.h>
#include <pthread.h>

using namespace rdmaPlus;

class App;

class Job {

	static const int NidRankId = 0x2;
	static const int NidPidId = 0x3;
	
    public:
        Job( Rdma& rdma, JobId jobId );
        ~Job();
        static void msg( Rdma&, ProcId src, JobMsg& msg );

    private:
        Job( const Job& );
        static void* thread( void* );

        bool msg( ProcId src, JobMsg& msg );
	    static void* sendCallback(void*,void*);

        typedef std::pair<Request*,CtrlMsg*> sendCbData_t;

        void* sendCallback(sendCbData_t*);

        bool writeAppInfo( int nRanks, int myRank, int ranksPer, ProcId* );

        bool load( ProcId src, struct JobMsg::Load& );
        bool kill( struct JobMsg::Kill& );
        bool nidPidMapAll( ProcId src, struct JobMsg::NidPidMapAll& );

        bool childExitMsg( struct JobMsg::ChildExit& );
        bool childStart( struct JobMsg::ChildStart& );
        bool nidPidMapPart( ProcId src, struct JobMsg::NidPidMapPart& );
        bool gotStart();

        int sendChildExit();
        int sendNidPidMapPart();
        void sendChildStart( );
        bool sendGotStart( uint child );
        bool fanoutLoadMsg( struct JobMsg::Load&, NidRnk& );
        void fanoutPidMap();
        void fanoutKill( int signal );

        uint nRanksThisNode(); 
        uint baseRank();

        void start();
        bool sendCtrlMsg2Child( CtrlMsg&, int fromChild );
        bool sendCtrlMsg2Parent( CtrlMsg&, int toChild );
        bool sendCtrlMsg( CtrlMsg&, ProcId& dest );

        Rdma&               m_rdma;
        JobId               m_jobId;

        App*                m_app;
        std::vector<NidRnk> m_nidRnkMap;

        unsigned int        m_numChildStarted;
        unsigned int        m_numChildAlive;
        unsigned int        m_numLocalAlive;
        uint                m_havePidPart;
        uint                m_nChildRanks;
        uint                m_nChildNids;
        uint                m_stride;
        uint                m_childNum;

        std::vector< ProcId>    m_localNidMap;
        std::vector< bool >     m_childStartedV;
        Route		            m_route;

        typedef std::map<JobId,Job*> JobMap;
        static JobMap           m_jobMap;
        static pthread_mutex_t  m_mutex;
        pthread_t               m_thread;
};

#endif

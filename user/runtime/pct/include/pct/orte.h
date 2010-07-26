#ifndef _PCT_ORTE_H  
#define _PCT_ORTE_H  

#include <rdmaPlus.h>
#include <pct/msgs.h>

#include <pct/log.h>

struct LwkOrteRmlMsg;
class Route;

using namespace rdmaPlus;

class Orte {
    public:
	Orte(Rdma&, Route&, int baseRank, int ranksPer, std::vector< ProcId >& );
	~Orte();
    
    void initPid( int pid, int totalRanks, int myRank );
    void finiPid( int pid );
    void startThread();
    void stopThread();

    private:

	static void* recvCallback(void*,void*);
	static void* sendCallback(void*,void*);
    static void* thread(void*);
    void thread();
	void processOOBMsg( OrteMsg* msg );
	void processBarrierMsg( OrteMsg* msg );

	typedef std::pair<Request*,LwkOrteRmlMsg*> sendCbData_t;
	void* recvCallback(Request*);
	void* sendCallback(sendCbData_t*);
	void armRecv();
	void processRead( int fd );
	void processWrite( int fd );
	void barrier( LwkOrteRmlMsg* );
    void sendOOBMsg( LwkOrteRmlMsg* hdr, unsigned char * body );
    void sendBarrierMsg( ProcId&, OrteMsg::u::barrier::type_t );
    void sendMsg( OrteMsg*, ProcId& );
    void barrierFanout();

    std::vector< ProcId >&	m_localNidMap;
    Rdma&           m_dm;
    Route&          m_route;

    OrteMsg         m_msg;
    int             m_baseRank;
    int             m_ranksPer;
    int	            m_numChildren;
    int             m_barrierPendingCnt;
    bool            m_barrierRoot;
    std::map< int, std::pair<int,int> > m_pid2fdM;
    std::map< int, int >                m_rank2PidM; 

    pthread_mutex_t         m_mutex;
    pthread_t               m_thread;
    bool                    m_threadAlive;
#if 0
    Log<>           m_log;
#endif
};

#endif

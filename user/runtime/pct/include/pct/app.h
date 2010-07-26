#ifndef PCT_APP_H
#define PCT_APP_H

#include <sys/types.h>
#include <pct/msgs.h>
#include <pct/orte.h>
#include <pct/route.h>
#include <string>
#include <queue>

extern "C" {
    #include <lwk/task.h>
}

class App {
    public:
static const int ImageId = 0x1;
        App( Rdma&, Route& route, ProcId& srcId, JobMsg::Load::App& msg,
                            NidRnk&, std::vector< ProcId >& nidMap );
        ~App();
        bool Start( );
        ProcId& NidPidMap( uint pos );
        size_t NidPidMapSize();
        int Kill( int signal );

    private:

        App( const App& );

        int allocPids( int totalRanks, NidRnk& );

        typedef std::queue<paddr_t> AllocQ;

        bool run( int rankThisNode, id_t pid, uint cpu_id, user_cpumask_t );
        static paddr_t pmem_alloc( size_t len, size_t align, uintptr_t arg );

        struct PidCpu {
            pid_t  pid;
            uint cpu;
            user_cpumask_t cpumask;
        };

        std::vector<PidCpu> m_pidCpuMap;
        int                 m_uid;
        int                 m_gid;
        size_t              m_heap_len;
        size_t              m_stack_len;
        void*               m_elfAddr;
        char*               m_cmdAddr;
        std::vector<char*>  m_envAddrV;
        AllocQ              m_allocQ;
        ProcId*             m_nidPidMap;
        size_t              m_nidPidMapSize;
        uint*               m_runTimeInfo;
        size_t              m_runTimeInfoLen;
        Rdma&               m_rdma;
        Orte                m_orte;
};

#endif

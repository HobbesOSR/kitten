
#ifndef PCT_APP_H
#define PCT_APP_H

#include <sys/types.h>
#include <pct/msgs.h>
#include <string>
#include <queue>

extern "C" {
    #include <lwk/task.h>
}

class App {
    public:
        App( uint nRanks, size_t elf_len, size_t heap_len, size_t stack_len, 
                    uint cmdLine_len, uint env_len, uint uid, uint gid );
        ~App();
        bool Start( );
        void* ImageAddr();
        size_t ImageLen();
        ProcId& NidPidMap( uint pos );
        size_t NidPidMapSize();
        bool Exited();
        int Kill( int signal );
        int AllocPids( Nid nid, uint baseRank, uint nRanks );

    private:
        App( const App& );

        typedef std::queue<paddr_t> AllocQ;
        typedef std::queue<id_t>    PidQ;

        bool run( id_t pid, uint cpu_id );
        bool zombie( id_t pid );
        static paddr_t pmem_alloc( size_t len, size_t align, uintptr_t arg );

        struct PidCpu {
            Pid  pid;
            uint cpu;
        };

        std::vector<PidCpu> m_pidCpuMap;
        start_state_t       m_start_state;
        size_t              m_heap_len;
        size_t              m_stack_len;
        void*               m_elfAddr;
        char*               m_cmdAddr;
        char*               m_envAddr;
        AllocQ              m_allocQ;
        // this is redundant, use m_pidCpuMap
        PidQ                m_pidQ;
        size_t              m_imageLen;
        ProcId*             m_nidPidMap;
        size_t              m_nidPidMapSize;
        uint*               m_runTimeInfo;
        size_t              m_runTimeInfoLen;
};

#endif

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <pct/app.h>
#include <pct/pmem.h>
#include <pct/debug.h>
#include <pct/mem.h>
#include <lwk/autoconf.h>
#include <lwk/cpu.h>
#include <arch/types.h>
#include <lwk/macros.h>
#include <stdint.h>

extern "C" {
    #include <pct/elf.h>
}

App::App( Rdma& rdma, Route& route, ProcId& srcId, JobMsg::Load::App& msg,
                    NidRnk& nidRnk, std::vector< ProcId >& nidMap ):
    m_heap_len( msg.heap_len),
    m_stack_len( msg.stack_len),
    m_nidPidMapSize( msg.nRanks ),
    m_rdma( rdma ),
    m_orte( rdma, route, nidRnk.baseRank, nidRnk.ranksPer, nidMap )
{
    Debug( App, "nid=%#x baseRank=%d ranksThisNode=%d\n",
                    nidRnk.nid, nidRnk.baseRank, nidRnk.ranksPer );

    size_t  imageLen = msg.elf_len + msg.cmdLine_len +  msg.env_len;
    m_elfAddr = mem_alloc( imageLen );
    if ( m_elfAddr == NULL ) {
        throw -ENOMEM;
    }
        

    Debug( App, "elfAddr=%p\n", m_elfAddr );
    m_cmdAddr = (char*) ( (size_t) m_elfAddr + msg.elf_len );

    m_uid = msg.uid;
    m_gid = msg.gid;

    // alloc space to store the number of ranks and the nid/pid map
    // mem_alloc must return a pointer that's aligned on page size 
    // and the size must be aligned on PAGE_SIZE
    m_runTimeInfoLen = sizeof(uint) + 
                        m_nidPidMapSize * sizeof( ProcId ) + 
                        PAGE_SIZE;

    m_runTimeInfoLen &= ~(PAGE_SIZE - 1);
    
    m_runTimeInfo = (uint*) mem_alloc( m_runTimeInfoLen );

    if ( m_runTimeInfo == NULL ) {
        throw -ENOMEM;
    }

    *m_runTimeInfo = msg.nRanks;
    m_nidPidMap = (ProcId*)(m_runTimeInfo + 1);

    rdma.memRegionRegister( m_elfAddr, imageLen, ImageId );

    Debug(App,"read image\n");
    if ( rdma.memRead( srcId, msg.imageKey, msg.imageAddr,
                                            m_elfAddr, imageLen ) ) 
    {
        Warn( App, "image read failed\n" );
        throw;
    } 

    m_envAddrV.resize(nidRnk.ranksPer);
    m_envAddrV[0] = (char*) ( (size_t) m_cmdAddr + msg.cmdLine_len );

    for ( unsigned int i = 1; i < m_envAddrV.size(); i++ ) {
        m_envAddrV[i] = (char*) mem_alloc( msg.env_len );
        memcpy( m_envAddrV[i], m_envAddrV[0], msg.env_len );
    }

    allocPids( msg.nRanks, nidRnk ); 

    m_orte.startThread();
}

App::~App()
{
    Debug( App, "enter\n");

    m_orte.stopThread();

    // we should call finiPid as each rank dies or move this to Orte
    for ( unsigned int i = 0; i < m_pidCpuMap.size(); i++ ) {
        m_orte.finiPid( m_pidCpuMap[i].pid );
    }

    m_rdma.memRegionUnregister( ImageId );

    mem_free( m_runTimeInfo );
    mem_free( m_elfAddr );
    for ( unsigned int i = 1; i < m_envAddrV.size(); i++ ) {
        mem_free( m_envAddrV[i] );
    }

    while ( ! m_allocQ.empty() ) {
        _pmem_free( m_allocQ.front() );
        m_allocQ.pop();
    }

    for ( unsigned int i = 0; i < m_pidCpuMap.size(); i++ ) {
        if ( aspace_destroy( m_pidCpuMap[i].pid ) ) {
            Warn( App, "aspace_destroy( %d)\n", m_pidCpuMap[i].pid );
            throw -1;
        }
    }
    Debug( App, "return\n");
}

ProcId& App::NidPidMap( uint pos )
{
    return m_nidPidMap[ pos ];
}

size_t App::NidPidMapSize()
{
    return m_nidPidMapSize;
}

bool App::Start()
{
    Debug( App, "\n");
    for ( unsigned int i = 0; i < m_pidCpuMap.size(); i++ ) {
        if ( run( i, m_pidCpuMap[i].pid, m_pidCpuMap[i].cpu,
                                        m_pidCpuMap[i].cpumask ) )
        {
            Debug( App, "app->Run() failed\n");
            return -1;
        }
        Info( "started process %d\n", m_pidCpuMap[i].pid );
    }
    return 0;
}

int App::allocPids( int totalRanks, NidRnk& nidRnk )
{

    m_pidCpuMap.resize( nidRnk.ranksPer );

    unsigned int i;
    for ( i = 0; i < nidRnk.ranksPer; i++ ) {
        m_pidCpuMap[i].pid = i + 100; // start app pids at 100
	// FIXME: assuming 8 cpus, wrap
        m_pidCpuMap[i].cpu = (i + 1)%8;

        m_orte.initPid( m_pidCpuMap[i].pid, totalRanks, nidRnk.baseRank + i );

        m_nidPidMap[nidRnk.baseRank + i].pid = m_pidCpuMap[i].pid; 
        m_nidPidMap[nidRnk.baseRank + i].nid = nidRnk.nid; 
        Debug(App,"pid=%d cpu=%d\n", m_pidCpuMap[i].pid, m_pidCpuMap[i].cpu );
    }
    return 0;
}

int App::Kill( int signal )
{
    Debug( App, "\n");

    for ( unsigned int i = 0; i < m_pidCpuMap.size(); i++ ) {
        if ( kill( m_pidCpuMap[i].pid, signal ) ) {
            Debug( App,"task_kill() failed\n" );
        }
    }
    return 0;
}

bool App::run( int rankThisNode, id_t pid, uint cpu_id, user_cpumask_t cpumask )
{
    start_state_t   start_state;
    Debug( App, "pid=%d cpu_id=%d\n", pid, cpu_id );
    Debug( App, "argv=%s\n",m_cmdAddr);
    //Debug( App, "envp=%s\n",m_envAddr);

#if DEBUG
    if ( elf_check_hdr( ( elfhdr*) elf_image ) == 0 ) {
        elf_print_elfhdr( (elfhdr*) elf_image );
    }
#endif

    std::string argv = m_cmdAddr;
    std::string name = argv.substr( 0, argv.find(" ") );
    argv = argv.substr( name.size(), argv.size() );

    start_state.cpu_id  = cpu_id;
    start_state.task_id  = pid;
    start_state.user_id  = 1;
    start_state.group_id  = 1;
    start_state.user_id = m_uid;
    start_state.group_id = m_gid;
    //start_state.cpumask = cpumask;

    sprintf(start_state.task_name, "user-%d", pid );

    int rc;

    if ( ( rc = elf_load( 
                    m_elfAddr, 
                    name.c_str(), 
                    pid, // aspace id is the same as the pid 
                    PAGE_SIZE,
                    m_heap_len,
                    m_stack_len,
                    (char*) argv.data(),
                    m_envAddrV[rankThisNode],
                    &start_state,
                    (uintptr_t) &m_allocQ,
                    pmem_alloc) ) ) 
    {
        Warn( App, "elf_load() failed\n");
        return rc;
    }

    if ( aspace_map_region( pid, (vaddr_t) m_runTimeInfo, m_runTimeInfoLen, 
                        (VM_USER|VM_READ), PAGE_SIZE, 
                        "NidPidMap", mem_paddr( (void*) m_runTimeInfo ) ) ) 
    {
        Warn( App, "failed to map nidpidmap in to aspace\n");
        return -1;
    }

    id_t gotId;
    rc = task_create( &start_state, &gotId );
    if ( rc || pid != gotId ) {
        Warn( App, "task_create() failed\n");
        return  -1;
    }
    return 0;
}

paddr_t App::pmem_alloc( size_t len, size_t alignment, uintptr_t arg )
{
    AllocQ* allocQ = (AllocQ*)arg;
    paddr_t paddr = _pmem_alloc( len, alignment, 0 );
    allocQ->push( paddr );
    return paddr;
} 

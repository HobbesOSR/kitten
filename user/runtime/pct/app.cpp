
#include <stdio.h>
#include <errno.h>

#include <pct/app.h>
#include <pct/pmem.h>
#include <pct/debug.h>
#include <pct/mem.h>

extern "C" {
    #include <pct/elf.h>
}

static int findCpu( unsigned int start, user_cpumask_t mask )
{
    for ( int i = start; i <= CPU_MAX_ID; i++) {
        if (cpu_isset(i, mask)) {
            return i;
        }
    }
    return -1;
}


App::App( uint nRanks, size_t elf_len, size_t heap_len, size_t stack_len, 
            uint cmdLine_len, uint env_len, uint uid, uint gid ) :
    m_heap_len(heap_len),
    m_stack_len(stack_len),
    m_imageLen( elf_len + cmdLine_len +  env_len ),
    m_nidPidMapSize( nRanks )
{

    Debug( App, "heap_len=%lu stack_len=%lu\n",m_heap_len,m_stack_len);

    m_elfAddr = mem_alloc( elf_len + cmdLine_len + env_len );
    if ( m_elfAddr == NULL ) {
        throw -ENOMEM;
    }
    Debug( App, "elfAddr=%p\n", m_elfAddr );
    m_cmdAddr = (char*) ( (size_t) m_elfAddr + elf_len );
    m_envAddr = (char*) ( (size_t) m_cmdAddr + cmdLine_len );
    m_start_state.user_id = uid;
    m_start_state.group_id = gid;

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

    *m_runTimeInfo = nRanks;
    m_nidPidMap = (ProcId*)(m_runTimeInfo + 1);
}

App::~App()
{
    Debug( App, "enter\n");

    mem_free( m_runTimeInfo );
    mem_free( m_elfAddr );

    while ( ! m_allocQ.empty() ) {
        _pmem_free( m_allocQ.front() );
        m_allocQ.pop();
    }

    while ( ! m_pidQ.empty() ) {
        int rc;
        Debug( App, "calling task_destroy( %d )\n", m_pidQ.front() );
        if ( ( rc = task_destroy( m_pidQ.front() ) ) ) {
            throw rc;
        }
        m_pidQ.pop();
    }
}


void* App::ImageAddr()
{
    return m_elfAddr;
}

size_t App::ImageLen()
{
    return m_imageLen;
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
        if ( run( m_pidCpuMap[i].pid, m_pidCpuMap[i].cpu ) )
        {
            Debug( App, "app->Run() failed\n");
            return -1;
        }
    }
    return 0;
}


int App::AllocPids( Nid nid, uint baseRank, uint nRanks )
{
    int current = -1;
    user_cpumask_t my_cpumask;

    if ( task_get_cpumask(&my_cpumask) ) {
        Warn( App, "\n");
        return -1;
    }

    m_pidCpuMap.resize( nRanks );

    static int Cur = 0;

    for ( unsigned int i = 0; i < nRanks; i++ ) {
        current = findCpu( current + 1, my_cpumask );
        if ( current == -1 ) {
            Warn( App, "\n");
            m_pidCpuMap.clear();
            return -1;
        }
        m_pidCpuMap[i].pid = i + UASPACE_MIN_ID + Cur++;
        m_pidCpuMap[i].cpu = current;
        m_nidPidMap[baseRank + i].pid = m_pidCpuMap[i].pid; 
        m_nidPidMap[baseRank + i].nid = nid; 

    }
    return 0;
}

int App::Kill( int signal )
{
    Debug( App, "\n");

    for ( unsigned int i = 0; i < m_pidCpuMap.size(); i++ ) {
        if ( task_kill( m_pidCpuMap[i].pid, signal ) ) {
            Debug( App,"task_kill() failed\n" );
        }
    }
    return 0;
}

bool App::Exited()
{
    // has the app started 
    if ( m_pidQ.size() != m_pidCpuMap.size() ) return false;

    for ( unsigned int i = 0; i < m_pidCpuMap.size(); i++ ) {
        if( ! zombie( m_pidCpuMap[i].pid ) )
            return false;
    }
    //Debug(App,"\n");
    return true;
}

bool App::run( id_t pid, uint cpu_id )
{
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

    m_start_state.cpu_id  = cpu_id;
    m_start_state.task_id  = pid;
    m_start_state.user_id  = 1;
    m_start_state.group_id  = 1;

    sprintf(m_start_state.task_name, "user-%d", pid );
    task_get_cpumask( & m_start_state.cpumask );
    //Debug( App, "%s %s\n", name.c_str(), argv.c_str() );

    int rc;

    if ( ( rc = elf_load( 
                    m_elfAddr, 
                    //mem_paddr(m_elfAddr),
                    name.c_str(), 
                    pid, // aspace id is the same as the pid 
                    PAGE_SIZE,
                    m_heap_len,
                    m_stack_len,
                    (char*) argv.data(),
                    m_envAddr,
                    &m_start_state,
                    (uintptr_t) &m_allocQ,
                    pmem_alloc) ) ) 
    {
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
    //rc = task_create( pid, name.c_str(), &m_start_state, &gotId );
    rc = task_create( &m_start_state, &gotId );
    if ( rc || pid != gotId ) {
        return  -1;
    }
    m_pidQ.push( gotId );
    return 0;
}

bool App::zombie( id_t pid )
{
    return task_is_zombie( pid );
}

paddr_t App::pmem_alloc( size_t len, size_t alignment, uintptr_t arg )
{
    AllocQ* allocQ = (AllocQ*)arg;
    paddr_t paddr = _pmem_alloc( len, alignment, 0 );
    allocQ->push( paddr );
    return paddr;
} 

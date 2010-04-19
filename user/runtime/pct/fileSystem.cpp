#include <pct/fileSystem.h>
#include <pct/debug.h>

extern "C" {
#include <lwk/liblwk.h>
}

FileSystem::FileSystem( RDMA& rdma, RDMA::nodeId_t server, 
                                    const std::string path ) : 
    m_rdma( rdma ),
    m_server( server )
{
    Debug( FileSystem, "\n");
    if ( ( m_fileSystemQ =  lwk_scq_create() ) < 0 ) {
        throw -1;
    } 
}

FileSystem::~FileSystem()
{
    Debug( FileSystem, "\n");
    lwk_scq_destroy( m_fileSystemQ );
}

bool FileSystem::Work()
{
    struct lwk_syscall syscall;
    if ( lwk_scq_pop( m_fileSystemQ, &syscall ) > 0 ) {

        Debug( FileSystem, "SYSCALL: type=%d tid=%d\n", 
                                            syscall.type, syscall.tid );

        if ( do_syscall(&syscall) ) return true;
    }
    return false;
}

#include <lwk/unistd.h>

int FileSystem::do_syscall( struct lwk_syscall *s )
{
    switch( s->type ) {

        case __NR_write:
            return do_write( s->tid, (int) s->arg[0], (char *) s->arg[1],
        (size_t) s->arg[2] );
            break;

        case __NR_read:
            return do_read( s->tid, (int) s->arg[0], (char *) s->arg[1],
        (size_t) s->arg[2] );
            break;

        case __NR_open:
            return do_open( s->tid, (char*) s->arg[0], (int) s->arg[1],
            (mode_t) s->arg[2] );
            break;

        default:
            Warn( FileSystem, "don't know about syscall type %d\n",s->type);
            return -1;
            //return do_exit(s->tid, 0 );
    }
}

#include <pct/fs_msgs.h>
#include <pct/msgs.h>

int FileSystem::do_open( int tid, const char *path, int flags, mode_t mode )
{
    //File* file = new File();

    //FileKey key( tid, ); 
    //m_fileMap.insert(, std::make_pair( key ,file ) )
    return 0;
}

int FileSystem::do_read( int tid, int lwk_fd, char *buf, size_t size )
{
    return 0;
}
int FileSystem::do_write( int tid, int lwk_fd, char *buf, size_t size )
{
    Debug( FileSystem, "tid=%d lwk_fd=%d buf=%p size=%ld\n", 
                                            tid, lwk_fd, buf, size );

    printf("%lu\n", write(1,buf,size) );
    
    struct SysCallReq msg;
    msg.syscall   = __NR_write;
    msg.pid       = tid;

    RDMA::handle_t region;
    m_rdma.Register( buf, size, 0, &region );

    msg.u.write.rdma_key  = m_rdma.Key(region);
    msg.u.write.rdma_addr = 0;
    msg.u.write.len=size;

#if 0
    msg.u.write.off     = app_info.file[lwk_fd].off;
    msg.u.write.flags   = app_info.file[lwk_fd].flags;
    msg.u.write.handle  = app_info.file[lwk_fd].fd;
#endif

    msg.resp_key = respKey();

    RDMA::addr_t addr = 0 * sizeof(struct SysCallReq);

    m_rdma.Write( m_server, SYSCALL_MSG_KEY, addr, sizeof(msg), &msg );

    RDMA::event_t event;

    m_rdma.Event( region, &event );

#if 0
    if ( app_info.sc_resp.retval > 0 ) {
        app_info.file[lwk_fd].off = app_info.sc_resp.u.offset;
    }
#endi

    Debug( FileSystem, "sc_retval=%ld\n",app_info.sc_resp.retval);
#endif
    lwk_sc_finish( tid, -1, 1 );

    return 0;
}


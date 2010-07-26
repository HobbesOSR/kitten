#include <linux/file.h>
#include <linux/fs.h>
#include <lwk/kfs.h>

//#define dbg _KDBG
#define dbg(fmt,args...)

struct file *alloc_file(struct vfsmount *mnt, struct dentry *dentry,
                fmode_t mode, const struct file_operations *fop)
{
        struct file *file = kmem_alloc( sizeof(*file));

	dbg("file=%p\n",file);
	if ( !file)
		return NULL;

	memset(file,0,sizeof(*file));
	file->f_mode = mode;
	file->f_op = fop;
	atomic_set(&file->f_count,1);

        return file;
}

int get_unused_fd(void)
{
        int fd = fdTableGetUnused( current->fdTable );
        dbg("fd=%d\n",fd);
        return fd;
}

void put_unused_fd( unsigned int fd )
{
        dbg( "fd=%d file=%p\n", fd, fdTableFile( current->fdTable, fd ) );
        fdTableInstallFd( current->fdTable, fd, NULL );
}

struct file *fget( unsigned int fd )
{
	struct file* file = fdTableFile( current->fdTable, fd );
        int count = atomic_add_return( 1, &file->f_count );
        dbg( "fd=%d file=%p f_count=%d\n", fd, file, count );
        return file;
}

void fput( struct file * filep )
{
        /* sync an release everything tied to the file */
        dbg( "file=%p\n", filep );
        if ( atomic_dec_and_test( &filep->f_count ) ) {
        	dbg( "f_count is 0\n" );
#if 0
	        if ( filep->f_op->release ) {
        		dbg( "release\n" );
                	filep->f_op->release( filep->inode, filep);
		}
#endif
		//kmem_free( filep );
	}
}

void fd_install(unsigned int fd, struct file *file)
{
        dbg("fd=%d file=%p\n",fd,file);
        fdTableInstallFd( current->fdTable, fd, file );
}

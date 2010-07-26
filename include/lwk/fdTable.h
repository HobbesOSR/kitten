#ifndef _LWK_FDTABLE_H
#define _LWK_FDTABLE_H

// Maximum files opened at one time
#define MAX_FILES       32

#include <lwk/spinlock.h>

struct file;
struct fdTable {
        struct file* files[ MAX_FILES];
        int ref_count;
	spinlock_t	lock;
};

static inline struct fdTable* fdTableAlloc( void )
{
	struct fdTable* tbl = kmem_alloc( sizeof( struct fdTable ) );
	//_KDBG("\n");
	memset( tbl, 0, sizeof( *tbl ) );
	tbl->ref_count = 1;
	spin_lock_init(&tbl->lock );
	return tbl;
}

static inline void fdTableFree( struct fdTable* tbl )
{
	spin_lock( &tbl->lock );

	//_KDBG("ref_count=%d\n",tbl->ref_count);

	--tbl->ref_count;
	if ( tbl->ref_count == 0  ) {
		kmem_free( tbl ); 
	}
	spin_unlock( &tbl->lock );
}

static inline struct fdTable* fdTableClone( struct fdTable* tbl )
{
	spin_lock( &tbl->lock );
	++tbl->ref_count;
	//_KDBG("ref_count=%d\n",tbl->ref_count);
	spin_unlock( &tbl->lock );
	return tbl;
}


static inline struct file* fdTableFile( struct fdTable* tbl, int fd )
{
        if( fd < 0 || fd > MAX_FILES )
                return NULL;

        struct file* file;

        spin_lock( &tbl->lock );
        file = tbl->files[ fd ];
        spin_unlock( &tbl->lock );
        return file;
}


static inline void fdTableInstallFd( struct fdTable* tbl, int fd, struct file* file )
{
	spin_lock( &tbl->lock );
	//_KDBG("fd=%d new=%p old=%p\n",fd,file,tbl->files[fd]);
	tbl->files[fd] = file;
	spin_unlock( &tbl->lock );
}

static inline int fdTableGetUnused( struct fdTable* tbl )
{
        int fd;
	spin_lock( &tbl->lock );
        for( fd=0 ; fd<MAX_FILES ; fd++ )
        {
                if( ! tbl->files[fd] ) {
			tbl->files[fd] = (void*) 1;
                        break;
		}
        }
        //_KDBG("fd=%d\n", fd );
	spin_unlock( &tbl->lock );

        if( fd == MAX_FILES )
                return -EMFILE;
	return fd;
}
extern void fdTableClose( struct fdTable* tbl );

// this is the linux interface
extern int get_unused_fd(void);
extern void put_unused_fd(unsigned int fd);
extern struct file *fget(unsigned int fd);
extern void fput(struct file * filep);
extern void fd_install(unsigned int fd, struct file *file);

#endif

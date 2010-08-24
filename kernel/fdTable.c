#include <lwk/kfs.h>

//#define dbg _KDBG
#define dbg(fmt,args...)

void fdTableClose( struct fdTable* tbl )
{
	spin_lock( &tbl->lock );
	dbg("\n");
	int i;
	for ( i = 0; i < MAX_FILES; i++ ) {
		struct file* file =  tbl->files[i];
		if ( file ) {
			dbg("fd=%d file=%p f_count=%d\n", i, file, 
				atomic_read( &file->f_count) );

	        	if ( atomic_dec_and_test( &file->f_count ) ) {
                		if ( file->f_op->release ) {
                        		dbg( "release %p\n",
							file->f_op->release );
                        		file->f_op->release( file->inode, file);
				}
                	}
			kfs_close( file );
        	}
	}
	dbg("\n");
	spin_unlock( &tbl->lock );
}

#ifndef _GDBIO_H_
#define _GDBIO_H_

#include <lwk/kernel.h>
#include <lwk/poll.h>
#include <lwk/kfs.h>
#include <lwk/sched.h>
#include <lwk/htable.h>

#include <arch/unistd.h>
#include <arch/uaccess.h>
#include <arch/atomic.h>

#define GDBIO_BUFFER_SIZE   4096
#define GDB_HTABLE_ORDER    6

struct gdbio_buffer {
	unsigned char *buf;
    waitq_t poll_wait;
    
    atomic_t avail_space;
    atomic_t avail_data;

    int capacity;
    int rindex;
    int windex;
};

struct gdb_fifo_file {
	struct gdbio_buffer*	in_buffer; //From the perspective of GDB server, it caches GDB requests
	struct gdbio_buffer*	out_buffer;//From the perspective of GDB server, it caches GDB replies
};

struct gdb_fifo_inode_priv {
	struct gdb_fifo_file*	file_ptr;
};

struct file *gdbio_connect(char *gdbcons);  //called by gdb client
void gdbio_detach(struct file *filep);      //called by gdb server stub
ssize_t gdbclient_read(struct file *filep, char *buffer, ssize_t len);
ssize_t gdbclient_write(struct file *filep, char *buffer, ssize_t size);
ssize_t gdbserver_read(struct file *filep, char *buffer, ssize_t len);
ssize_t gdbserver_write(struct file *filep, const char *buffer, ssize_t size);

struct file* open_gdb_fifo(const char *gdb_cons);
int mk_gdb_fifo(const char* name);
void rm_gdb_fifo(struct file *filep);    

#endif

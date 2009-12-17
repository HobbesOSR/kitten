#ifndef _LINUX_POLL_H
#define _LINUX_POLL_H

/* asm/poll.h */
/* These are specified by iBCS2 */
#define POLLIN          0x0001
#define POLLPRI         0x0002
#define POLLOUT         0x0004
#define POLLERR         0x0008
#define POLLHUP         0x0010
#define POLLNVAL        0x0020

/* The rest seem to be more-or-less nonstandard. Check them! */
#define POLLRDNORM      0x0040
#define POLLRDBAND      0x0080
#ifndef POLLWRNORM
#define POLLWRNORM      0x0100
#endif


struct poll_table_struct;

typedef void (*poll_queue_proc)(struct file *, wait_queue_head_t *, struct poll_table_struct *);

typedef struct poll_table_struct {
        poll_queue_proc qproc;
        unsigned long key;
} poll_table;

static inline void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
{
        if (p && wait_address)
                p->qproc(filp, wait_address, p);
}


#endif

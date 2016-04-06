#include <lwk/poll.h>
#include <lwk/aspace.h>
#include <arch/uaccess.h>

struct pollfd {
	int   fd;         /* file descriptor */
	short events;     /* requested events */
	short revents;    /* returned events */
};

struct poll_list {
	struct poll_list *next;
	int len;
	struct pollfd entries[0];
};

struct poll_table_page {
	struct poll_table_page * next;
	struct poll_table_entry * entry;
	struct poll_table_entry entries[0];
};

typedef unsigned long int nfds_t;
#define POLLFD_PER_PAGE  ((PAGE_SIZE-sizeof(struct poll_list)) / \
				sizeof(struct pollfd))
#define N_STACK_PPS ((sizeof(stack_pps) - sizeof(struct poll_list))  /	\
			sizeof(struct pollfd))
#define POLL_TABLE_FULL(table) \
	((unsigned long)((table)->entry+1) > PAGE_SIZE + (unsigned long)(table))

static struct poll_table_entry *poll_get_entry(struct poll_wqueues *p)
{
	struct poll_table_page *table = p->table;

	if (p->inline_index < N_INLINE_POLL_ENTRIES)
		return p->inline_entries + p->inline_index++;

	if (!table || POLL_TABLE_FULL(table)) {
		struct poll_table_page *new_table;

		new_table = (struct poll_table_page *) kmem_get_pages(0);
		if (!new_table) {
			p->error = -ENOMEM;
			return NULL;
		}
		new_table->entry = new_table->entries;
		new_table->next = table;
		p->table = new_table;
		table = new_table;
	}

	return table->entry++;
}


static int __pollwake(waitq_entry_t *wait, unsigned mode, int sync, void *key)
{
	struct poll_wqueues *pwq = wait->private;

	/*
	 * Although this function is called under waitqueue lock, LOCK
	 * doesn't imply write barrier and the users expect write
	 * barrier semantics on wakeup functions.  The following
	 * smp_wmb() is equivalent to smp_wmb() in try_to_wake_up()
	 * and is paired with set_mb() in poll_schedule_timeout.
	 */
	smp_wmb();
	pwq->triggered = 1;

	/*
	 * Perform the default wake up operation using a dummy
	 * waitqueue.
	 *
	 * TODO: This is hacky but there currently is no interface to
	 * pass in @sync.  @sync is scheduled to be removed and once
	 * that happens, wake_up_process() can be used directly.
	 */
	return sched_wakeup_task(pwq->polling_task, TASK_ALL);
}

static int pollwake(waitq_entry_t *wait, unsigned mode, int sync, void *key)
{
	struct poll_table_entry *entry;

	entry = container_of(wait, struct poll_table_entry, wait);
	if (key && !((unsigned long)key & entry->key))
		return 0;
	return __pollwake(wait, mode, sync, key);
}

/* Add a new entry */
static void __pollwait(struct file *filp, waitq_t *wait_address,
		       poll_table *p)
{
	struct poll_wqueues *pwq = container_of(p, struct poll_wqueues, pt);
	struct poll_table_entry *entry = poll_get_entry(pwq);
	if (!entry)
		return;
	//get_file(filp); - add to f_count - not in kitten
	entry->filp = filp;
	entry->wait_address = wait_address;
	entry->key = p->key;
	entry->wait.private = NULL;
	entry->wait.func = pollwake;
	list_head_init(&entry->wait.link);

	entry->wait.private = pwq;
	waitq_add_entry(wait_address, &entry->wait);
}

static long estimate_accuracy(struct timespec *tv)
{
	/*
	 * TODO: Using realtime setting - because of simplicity.
	 * Realtime tasks get a slack of 0 for obvious reasons.
	 */
	return 0;
}

void poll_initwait(struct poll_wqueues *pwq)
{
	init_poll_funcptr(&pwq->pt, __pollwait);
	pwq->polling_task = current;
	pwq->triggered = 0;
	pwq->error = 0;
	pwq->table = NULL;
	pwq->inline_index = 0;
}

static void free_poll_entry(struct poll_table_entry *entry)
{
	waitq_remove_entry(entry->wait_address, &entry->wait);
	//fput(entry->filp);
}

void poll_freewait(struct poll_wqueues *pwq)
{
	struct poll_table_page * p = pwq->table;
	int i;
	for (i = 0; i < pwq->inline_index; i++)
		free_poll_entry(pwq->inline_entries + i);
	while (p) {
		struct poll_table_entry * entry;
		struct poll_table_page *old;

		entry = p->entry;
		do {
			entry--;
			free_poll_entry(entry);
		} while (entry > p->entries);
		old = p;
		p = p->next;
		kmem_free_pages(old, 0);
	}
}

int poll_schedule_timeout(struct poll_wqueues *pwq, int state,
			  ktime_t *expires, unsigned long slack)
{
	int rc = -EINTR;

	set_current_state(state);
	if (!pwq->triggered) {
		if (!expires) {
			/* wait indefinitely */
			schedule();
		} else {
			schedule_timeout(*expires);
		}

		/* Assume that if there is no signal pending, we were woken
		 * up either because one of the things we are waiting on
		 * happened or because a timeout occurred. In either case,
		 * we return 0 to the caller. */
		if (!signal_pending(current)) {
			rc = 0;
		}
	}
	__set_current_state(TASK_RUNNING);

	/*
	 * Prepare for the next iteration.
	 *
	 * The following set_mb() serves two purposes.  First, it's
	 * the counterpart rmb of the wmb in pollwake() such that data
	 * written before wake up is always visible after wake up.
	 * Second, the full barrier guarantees that triggered clearing
	 * doesn't pass event check of the next iteration.  Note that
	 * this problem doesn't exist for the first iteration as
	 * add_wait_queue() has full barrier semantics.
	 */
	set_mb(pwq->triggered, 0);

	return rc;
}


/*
 * Fish for pollable events on the pollfd->fd file descriptor. We're only
 * interested in events matching the pollfd->events mask, and the result
 * matching that mask is both recorded in pollfd->revents and returned. The
 * pwait poll_table will be used by the fd-provided poll handler for waiting,
 * if non-NULL.
 */
static inline unsigned int do_pollfd(struct pollfd *pollfd, poll_table *pwait)
{
	unsigned int mask;
	int fd;

	mask = 0;
	fd = pollfd->fd;
	if (fd >= 0) {
		//int fput_needed;
		struct file * file;

		file = get_current_file( fd ); //fget_light(fd, &fput_needed);
		mask = POLLNVAL;
		if (file != NULL) {
			mask = DEFAULT_POLLMASK;
			if (file->f_op && file->f_op->poll) {
				if (pwait)
					pwait->key = pollfd->events |
							POLLERR | POLLHUP;
				mask = file->f_op->poll(file, pwait);
			}
			/* Mask out unneeded events. */
			mask &= pollfd->events | POLLERR | POLLHUP;
			//fput_light(file, fput_needed);
		}
	}
	pollfd->revents = mask;

	return mask;
}

static int do_poll(unsigned int nfds,  struct poll_list *list,
		   struct poll_wqueues *wait, struct timespec *end_time)
{
	poll_table* pt = &wait->pt;
	ktime_t expire, *to = NULL;
	int timed_out = 0, count = 0;
	unsigned long slack = 0;

	/* Optimise the no-wait case */
	if (end_time && !end_time->tv_sec && !end_time->tv_nsec) {
		pt = NULL;
		timed_out = 1;
	}

	if (end_time && !timed_out)
		slack = estimate_accuracy(end_time);

	for (;;) {
		struct poll_list *walk;

		for (walk = list; walk != NULL; walk = walk->next) {
			struct pollfd * pfd, * pfd_end;

			pfd = walk->entries;
			pfd_end = pfd + walk->len;
			for (; pfd != pfd_end; pfd++) {
				/*
				 * Fish for events. If we found one, record it
				 * and kill the poll_table, so we don't
				 * needlessly register any other waiters after
				 * this. They'll get immediately deregistered
				 * when we break out and return.
				 */
				if (do_pollfd(pfd, pt)) {
					count++;
					pt = NULL;
				}
			}
		}

		/*
		 * All waiters have already been registered, so don't provide
		 * a poll_table to them on the next loop iteration.
		 */
		pt = NULL;
		if (!count) {
			count = wait->error;
			if (signal_pending(current)) {
				count = -EINTR;
			}
		}

		if (count || timed_out)
			break;

		/*
		 * If this is the first loop and we have a timeout
		 * given, then we convert to ktime_t and set the to
		 * pointer to the expiry value.
		 */
		if (end_time && !to) {
			expire = timespec_to_ktime(*end_time);
			to = &expire;
		}

		if (!poll_schedule_timeout(wait, TASK_INTERRUPTIBLE, to, slack)) {
			timed_out = 1;
		}
	}

	return count;
}


int
sys_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	struct poll_wqueues table;
	struct timespec end_time = { 0, 0 }, *end_time_p = NULL;
	int len, fdcount, size, err = -EFAULT;
	long stack_pps[256/sizeof(long)];
	struct poll_list *const head = (struct poll_list *)stack_pps;
	struct poll_list *walk = head;
	unsigned long todo = nfds;

	if (timeout >= 0) {
		/* timeout given in ms */
		end_time.tv_sec = timeout / MSEC_PER_SEC;
		end_time.tv_nsec = NSEC_PER_MSEC * (timeout % MSEC_PER_SEC);
		if (!timespec_valid(&end_time))
			return -EINVAL;
		end_time_p = &end_time;
	}
	/* timeout < 0 signifies infinite timeout: end_time_p remains NULL */

	len = min_t(unsigned int, nfds, N_STACK_PPS);
	for (;;) {
		walk->next = NULL;
		walk->len = len;
		if (!len)
			break;

		if (copy_from_user(walk->entries, fds + nfds-todo,
					sizeof(struct pollfd) * walk->len))
			goto out_fds;

		todo -= walk->len;
		if (!todo)
			break;

		len = min(todo, POLLFD_PER_PAGE);
		size = sizeof(struct poll_list) + sizeof(struct pollfd) * len;
		walk = walk->next = kmem_alloc(size);
		if (!walk) {
			err = -ENOMEM;
			goto out_fds;
		}
	}

	poll_initwait(&table);
	fdcount = do_poll(nfds, head, &table, end_time_p);
	poll_freewait(&table);

	for (walk = head; walk; walk = walk->next) {
		struct pollfd *wfds = walk->entries;
		int j;

		for (j = 0; j < walk->len; j++, fds++)
			if (__put_user(wfds[j].revents, &fds->revents))
				goto out_fds;
	}

	err = fdcount;
out_fds:
	walk = head->next;
	while (walk) {
		struct poll_list *pos = walk;
		walk = walk->next;
		kmem_free(pos);
	}

	return err;
}

int
sys_ppoll(struct pollfd __user *, ufds, unsigned int, nfds,
		struct timespec __user *, tsp, const sigset_t __user *, sigmask,
		size_t, sigsetsize)
{
	sigset_t ksigmask, sigsaved;
	struct timespec ts, end_time, *to = NULL;
	int ret;

	if (tsp) {
		if (copy_from_user(&ts, tsp, sizeof(ts)))
			return -EFAULT;

		to = &end_time;
		if (poll_select_set_timeout(to, ts.tv_sec, ts.tv_nsec))
			return -EINVAL;
	}

	if (sigmask) {
		/* XXX: Don't preclude handling different sized sigset_t's.  */
		if (sigsetsize != sizeof(sigset_t))
			return -EINVAL;
		if (copy_from_user(&ksigmask, sigmask, sizeof(ksigmask)))
			return -EFAULT;

		sigdelsetmask(&ksigmask, sigmask(SIGKILL)|sigmask(SIGSTOP));
		sigprocmask(SIG_SETMASK, &ksigmask, &sigsaved);
	}

	ret = do_poll(ufds, nfds, to);

	/* We can restart this syscall, usually */
	if (ret == -EINTR) {
		/*
		 * Don't restore the signal mask yet. Let do_signal() deliver
		 * the signal on the way back to userspace, before the signal
		 * mask is restored.
		 */
		if (sigmask) {
			memcpy(&current->saved_sigmask, &sigsaved,
					sizeof(sigsaved));
			set_restore_sigmask();
		}
		ret = -ERESTARTNOHAND;
	} else if (sigmask)
		sigprocmask(SIG_SETMASK, &sigsaved, NULL);

	ret = poll_select_copy_remaining(&end_time, tsp, 0, ret);

	return ret;
}

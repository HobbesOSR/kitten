#ifndef _LWK_SIGNAL_H
#define _LWK_SIGNAL_H

#include <lwk/list.h>
#include <lwk/sigset.h>
#include <lwk/idspace.h>
#include <arch/signal.h>
#include <arch/siginfo.h>


// Holds info for each pending signal
struct sigpending {
	struct list_head	list;	// of sigqueue structs
	sigset_t 		sigset;
};


// Holds information about a pending signal
struct sigqueue {
	struct list_head	list;
	int			flags;
	siginfo_t		siginfo;
};


// API for manipulating and inspecting the caller's signal state
extern int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
extern int sigpending(sigset_t *set);
extern int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
extern int sigsuspend(const sigset_t *mask);
extern int sigsend(id_t aspace_id, id_t task_id, int signum, struct siginfo *siginfo);
extern int sigdequeue(siginfo_t *siginfo, struct sigaction *sigact);


// Convenience function, returns true if task may have a signal pending
extern bool signal_pending(struct task_struct *task);


#endif

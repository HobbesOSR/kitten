#include <lwk/signal.h>
#include <lwk/task.h>
#include <lwk/aspace.h>
#include <lwk/xcall.h>
#include <lwk/sched.h>


// Tests if the input value is a valid signal number
static bool
is_valid_signal(int signum)
{
	return ((signum >= 1) && (signum <= NUM_SIGNALS));
}


// Returns true if the default action for the signal is SIG_IGN
static bool
signal_ignored_by_default(int signum)
{
	if ((signum == SIGCHLD) || (signum == SIGCONT) ||
	    (signum == SIGURG)  || (signum == SIGWINCH))
		return true;
	else
		return false;
}


// Removes signals from a sigpending structure
// Returns true if any signals were found
// Requires that caller has the correct aspace->lock locked
static bool
remove_from_sigpending(struct sigpending *sigpending, sigset_t *sigset)
{
	sigset_t result;
	struct sigqueue *q, *n;

	sigset_and(&result, sigset, &sigpending->sigset);
	if (sigset_isempty(&result))
		return false;

	sigset_nand(&sigpending->sigset, &sigpending->sigset, sigset);
	list_for_each_entry_safe(q, n, &sigpending->list, list) {
		if (sigset_test(sigset, q->siginfo.si_signo)) {
			list_del_init(&q->list);
			kmem_free(q);
		}
	}

	return true;
}


// Examine and change blocked signals
int
sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	unsigned long irqstate;

	// Begin critical section
	spin_lock_irqsave(&current->aspace->lock, irqstate);

	if (oldset)
		sigset_copy(oldset, &current->sigblocked);

	if (set) {
		switch (how) {
			case SIG_BLOCK:
				sigset_or(&current->sigblocked, &current->sigblocked, set);
				break;

			case SIG_UNBLOCK:
				sigset_nand(&current->sigblocked, &current->sigblocked, set);
				break;

			case SIG_SETMASK:
				sigset_copy(&current->sigblocked, set);
				break;

			default:
				spin_unlock_irqrestore(&current->aspace->lock, irqstate);
				return -EINVAL;
		}
	}

	// End critical section
	spin_unlock_irqrestore(&current->aspace->lock, irqstate);

	return 0;
}


// Examine pending signals
int
sigpending(sigset_t *set)
{
	unsigned long irqstate;

	// Begin critical section
	spin_lock_irqsave(&current->aspace->lock, irqstate);

	sigset_or(set, &current->aspace->sigpending.sigset, &current->sigpending.sigset);
	sigset_and(set, set, &current->sigblocked);

	// End critical section
	spin_unlock_irqrestore(&current->aspace->lock, irqstate);

	return 0;
}


// Examine and change a signal action
int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	unsigned long irqstate;

	if (!is_valid_signal(signum))
		return -EINVAL;

	// Begin critical section
	spin_lock_irqsave(&current->aspace->lock, irqstate);

	if (sigset_haspending(&current->aspace->sigpending.sigset, &current->sigblocked) ||
	    sigset_haspending(&current->sigpending.sigset, &current->sigblocked)) {
		// If there might be a fatal signal pending, 
		// make sure we take it before changing the action
		spin_unlock_irqrestore(&current->aspace->lock, irqstate);
		return -ERESTARTNOINTR;
	}

	if (oldact)
		*oldact = current->aspace->sigaction[signum - 1];

	if (act) {
		current->aspace->sigaction[signum - 1] = *act;

		// POSIX 3.3.1.3:
		//  "Setting a signal action to SIG_IGN for a signal that is
		//   pending shall cause the pending signal to be discarded,
		//   whether or not it is blocked."
		//
		//  "Setting a signal action to SIG_DFL for a signal that is
		//   pending and whose default action is to ignore the signal
		//   (for example, SIGCHLD), shall cause the pending signal to
		//   be discarded, whether or not it is blocked"
		if ((act->sa_handler == SIG_IGN) || 
		    ((act->sa_handler == SIG_DFL) &&
		     signal_ignored_by_default(signum))) {

			sigset_t sigset;
			sigset_zero(&sigset);
			sigset_add(&sigset, signum);

			remove_from_sigpending(&current->aspace->sigpending, &sigset);

			struct task_struct *task;
			list_for_each_entry(task, &current->aspace->task_list, aspace_link)
				remove_from_sigpending(&task->sigpending, &sigset);
		}
	}

	// End critical section
	spin_unlock_irqrestore(&current->aspace->lock, irqstate);

	return 0;
}


static struct sigqueue *
sigqueue_alloc(void)
{
	struct sigqueue *sigq;

	sigq = kmem_alloc(sizeof(struct sigqueue));
	if (!sigq)
		return NULL;

	list_head_init(&sigq->list);

	return sigq;
}


static void
sigqueue_free(struct sigqueue *sigq)
{
	kmem_free(sigq);
}


int
sigsend(id_t aspace_id, id_t task_id, int signum, struct siginfo *siginfo)
{
	struct aspace *aspace;
	struct task_struct *task = NULL;
	struct sigpending *sigpend;
	struct sigqueue *sigq;
	int status = 0;
	unsigned long irqstate;
	struct task_struct *cur;

	if (!is_valid_signal(signum) || !siginfo)
		return -EINVAL;

	aspace = aspace_acquire(aspace_id);
	if (!aspace)
		return -ESRCH;

	// Begin critical section
	spin_lock_irqsave(&aspace->lock, irqstate);

	// Short-circuit if this is a non-RT signal and one is already queued
	if ((signum < SIGRTMIN) && sigset_test(&aspace->sigpending.sigset, signum))
		goto out;

	// If a specific task_id was given, lookup the target task
	if (task_id != ANY_ID) {
		list_for_each_entry(cur, &aspace->task_list, aspace_link) {
			if (cur->id == task_id) {
				task = cur;
				sigpend = &cur->sigpending;
				break;
			}
		}
		if (!task) {
			status = -ESRCH;
			goto out;
		}
	} else {
		sigpend = &aspace->sigpending;
	}

	sigq = sigqueue_alloc();

	// Real-time signals abort if the siginfo can't be queued
	if (!sigq && signum >= SIGRTMIN) {
		status = -EAGAIN;
		goto out;
	}

	// Queue the siginfo
	if (sigq) {
		copy_siginfo(&sigq->siginfo, siginfo);
		list_add_tail(&sigq->list, &sigpend->list);
	}

	// Mark the signum as pending
	sigset_add(&sigpend->sigset, signum);

	// Set flag indicating that a signal is pending
	if (task) {
		set_bit(TF_SIG_PENDING_BIT, &task->arch.flags);
	} else {
		list_for_each_entry(cur, &aspace->task_list, aspace_link)
			set_bit(TF_SIG_PENDING_BIT, &cur->arch.flags);
	}

	// Wake up sleeping tasks in TASK_INTERRUPTIBLE
	if (task && (task->state == TASK_INTERRUPTIBLE)) {
		set_task_state(task, TASK_RUNNING);
		xcall_reschedule(task->cpu_id);
	} else {
		list_for_each_entry(cur, &aspace->task_list, aspace_link) {
			if (cur->state == TASK_INTERRUPTIBLE) {
				set_task_state(cur, TASK_RUNNING);
				xcall_reschedule(cur->cpu_id);
			}
		}
	}

out:
	// End critical section
	spin_unlock_irqrestore(&aspace->lock, irqstate);
	aspace_release(aspace);
	return status;
}


// Dequeues the next signal from the sigpending passed in
// Requires that caller has the correct aspace->lock locked
static int
sigpending_dequeue(struct sigpending *sigpending, siginfo_t *siginfo)
{
	int signum;
	struct sigqueue *cur, *first = NULL;

	signum = sigset_getnext(&sigpending->sigset, &current->sigblocked);
	if (!signum)
		return 0;

	// Get the first siginfo corresponding to signum.
	// Also check if there is another siginfo for the same signum.
	list_for_each_entry(cur, &sigpending->list, list) {
		if (cur->siginfo.si_signo == signum) {
			if (first)
				goto still_pending;
			first = cur;
		}
	}

	sigset_del(&sigpending->sigset, signum);

	if (first) {
still_pending:
		list_del_init(&first->list);
		copy_siginfo(siginfo, &first->siginfo);
		sigqueue_free(first);
	} else {
		// No siginfo was found corresponding to signum,
		// so fake it, zeroing everything but si_signo.
		siginfo->si_signo = signum;
		siginfo->si_errno = 0;
		siginfo->si_code  = 0;
		siginfo->si_pid   = 0;
		siginfo->si_uid   = 0;
	}

	return signum;
}


// Returns the next signal to deliver to the caller
int
sigdequeue(siginfo_t *siginfo, struct sigaction *sigact)
{
	int signum;
	struct sigaction *sa;
	unsigned long irqstate;

	// Begin critical section
	spin_lock_irqsave(&current->aspace->lock, irqstate);

	while (true) {
		// Get the next signal to deliver,
		// Inspect task-private sigpending first, then aspace sigpending
		signum = sigpending_dequeue(&current->sigpending, siginfo);
		if (!signum)
			signum = sigpending_dequeue(&current->aspace->sigpending, siginfo);
		if (!signum)
			break; // will return 0

		sa = &current->aspace->sigaction[signum - 1];

		if (sa->sa_handler == SIG_IGN) {
			continue;
		} else if (sa->sa_handler == SIG_DFL) {
			// Handle SIG_DFL = ignore
			if (   (signum == SIGCONT)
			    || (signum == SIGCHLD)
			    || (signum == SIGWINCH)
			    || (signum == SIGURG)
			   )
				continue;

			// Handle SIG_DFL = fatal (everything else)
			spin_unlock_irqrestore(&current->aspace->lock, irqstate);
			task_exit_group(signum);  // This call does not return
		} else {
			// Return the handler
			*sigact = *sa;

			// Handle one-shot handlers
			if (sa->sa_flags & SA_RESETHAND)
				sa->sa_handler = SIG_DFL;

			break;
		}
	}

	// If all signals have been processed, clear TF_SIG_PENDING_BIT
	if (sigset_isempty(&current->aspace->sigpending.sigset)
	      && sigset_isempty(&current->sigpending.sigset))
		clear_bit(TF_SIG_PENDING_BIT, &current->arch.flags);

	// End critical section
	spin_unlock_irqrestore(&current->aspace->lock, irqstate);

	return signum;
}


// Convenience function, returns true if task may have a signal pending
bool
signal_pending(struct task_struct *task)
{
	return test_bit(TF_SIG_PENDING_BIT, &task->arch.flags) ? true : false;
}

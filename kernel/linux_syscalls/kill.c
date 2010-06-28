#include <lwk/kernel.h>
#include <lwk/signal.h>
#include <lwk/aspace.h>

long
sys_kill(
	int	pid,
	int	signum
)
{
	struct siginfo siginfo;
	id_t aspace_id = (id_t) pid;

	// LWK doesn't support process groups
	if (pid <= 0)
		return -EPERM;

	// Caller wants to verify that pid exists
	if ((pid > 0) && (signum == 0)) {
		struct aspace *aspace = aspace_acquire(aspace_id);
		if (!aspace)
			return -ESRCH;
		aspace_release(aspace);
		return 0;
	}

	// Only root can send non-loopback signals
	if ((current->uid != 0) && (current->aspace->id != aspace_id))
		return -EPERM;

	memset(&siginfo, 0, sizeof(struct siginfo));
	siginfo.si_signo = signum;
	siginfo.si_errno = 0;
	siginfo.si_code  = SI_USER;
	siginfo.si_pid   = (int)current->aspace->id;
	siginfo.si_uid   = (int)current->uid;

	return sigsend(aspace_id, ANY_ID, signum, &siginfo);
}

#ifndef _ARCH_X86_64_SIGNAL_H
#define _ARCH_X86_64_SIGNAL_H


// x86_64 supports 64 signals
#define NUM_SIGNALS	64


// Signal numbers names
#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
#define SIGPWR		30
#define SIGSYS		31
#define	SIGUNUSED	31
#define SIGRTMIN	32
#define SIGRTMAX	NUM_SIGNALS


// Values for sigprocmask() 'how' argument
#define SIG_BLOCK	0
#define SIG_UNBLOCK	1
#define SIG_SETMASK	2


// Signal handler function constants, for use in sigaction.sa_handler field
#define SIG_DFL		((sighandler_func_t *)  0)
#define SIG_IGN		((sighandler_func_t *)  1)
#define SIG_ERR		((sighandler_func_t *) -1)


// Values for sigaction.sa_flags
#define SA_SIGINFO	0x00000004	// Pass siginfo_t to signal handler
#define SA_NODEFER	0x40000000	// Don't block signal while it is being handled
#define SA_RESETHAND	0x80000000	// Reset to SIG_DFL on entry to handler
#define SA_RESTART	0x10000000	// Restart system call on signal return
#define SA_RESTORER	0x04000000	// Indicates that there is a restorer function


// Signal set type
typedef struct {
	unsigned long bitmap[BITS_TO_LONGS(NUM_SIGNALS)];
} sigset_t;


// Signal handler and restorer function typedefs
typedef void sighandler_func_t(int);
typedef void sigrestorer_func_t(void);


// Signal action type
struct sigaction {
	sighandler_func_t *	sa_handler;
	unsigned long		sa_flags;
	sigrestorer_func_t *	sa_restorer;
	sigset_t		sa_mask;
};


#endif

/*
 * Machine check handler.
 *
 * K8 parts Copyright 2002,2003 Andi Kleen, SuSE Labs.
 * Rest from unknown author(s).
 * 2004 Andi Kleen. Rewrote most of it.
 * Copyright 2008 Intel Corporation
 * Author: Andi Kleen
 */

/*
 * copied from linux-3.3
 */

#include <lwk/kfs.h>
#include <lwk/poll.h>
#include <lwk/smp.h>
#include <lwk/driver.h>
#include <lwk/stat.h>
#include <lwk/delay.h>
#include <lwk/workq.h>
#include <lwk/xcall.h>
#include <lwk/kallsyms.h>
#include <lwk/bootmem.h>
#include <lwk/timer.h>
#include <arch/ioctl.h>
#include <arch/idt_vectors.h>
#include <arch/unistd.h>

#include "mce-internal.h"

#define init_irq_work(x,y)
struct irq_work { };
static inline void irq_work_queue( struct irq_work* ptr )
{
    panic("%s()\n",__func__);
}

static inline u64 get_seconds(void)
{
    // get_time() returns nano seconds
    return get_time()/NSEC_PER_SEC;
}

static inline void bust_spinlocks(int yes)
{
    panic("%s()\n",__func__);
}

/* These functions deal with notifying the running application that an MCE
 * has occured. They are not implemented but keep them for future reference 
 */
#define set_thread_flag(x)
#define clear_thread_flag(x)
#define force_sig(x,y)

/* We don't currently support marking the kernel as tainted. 
 * Keep this in the code for future reference 
 */
#define add_taint(x)

/* Linux has this but I don't think we need it.
 * Make it a no-op and keep it in the code for future reference. 
 */
#define synchronize_sched()

#define touch_nmi_watchdog() panic("%s() not implemented\n",__func__);

int panic_timeout;

static DEFINE_MUTEX(mce_chrdev_read_mutex);

int mce_disabled __read_mostly;

#define SPINUNIT 100	/* 100ns */

atomic_t mce_entry;

/*
 * Tolerant levels:
 *   0: always panic on uncorrected errors, log corrected errors
 *   1: panic or SIGBUS on uncorrected errors, log corrected errors
 *   2: SIGBUS or log uncorrected errors (if possible), log corrected errors
 *   3: never panic or SIGBUS, log all errors (for testing only)
 */
static int			tolerant		__read_mostly = 1;
static int			banks			__read_mostly;
static int			rip_msr			__read_mostly;
static int			mce_bootlog		__read_mostly = -1;
static int			monarch_timeout		__read_mostly = -1;
static int			mce_panic_timeout	__read_mostly;
static int			mce_dont_log_ce		__read_mostly;
int				mce_cmci_disabled	__read_mostly;
int				mce_ignore_ce		__read_mostly;
int				mce_ser			__read_mostly;

struct mce_bank                *mce_banks		__read_mostly;

static unsigned long		mce_need_notify;

static DECLARE_WAITQ(mce_chrdev_wait);

static DEFINE_PER_CPU(struct mce, mces_seen);
static int			cpu_missing;

/* MCA banks polled by the period polling timer for corrected events */
DEFINE_PER_CPU(mce_banks_t, mce_poll_banks) = {
	[0 ... BITS_TO_LONGS(MAX_NR_BANKS)-1] = ~0UL
};

static DEFINE_PER_CPU(struct work_struct, mce_work);

/* Do initial initialization of a struct mce */
void mce_setup(struct mce *m)
{
	memset(m, 0, sizeof(struct mce));
	m->cpu = m->extcpu = smp_processor_id();
	rdtscll(m->tsc);
	/* We hope get_seconds stays lockless */
	m->time = get_seconds();
	m->cpuvendor = boot_cpu_data.arch.x86_vendor;
	m->cpuid = cpuid_eax(1);
#if 0 // kitten doesn't currently support these
	m->socketid = cpu_data(m->extcpu).phys_proc_id;
	m->apicid = cpu_data(m->extcpu).initial_apicid;
#endif
	rdmsrl(MSR_IA32_MCG_CAP, m->mcgcap);
}

DEFINE_PER_CPU(struct mce, injectm);

/*
 * Lockless MCE logging infrastructure.
 * This avoids deadlocks on printk locks without having to break locks. Also
 * separate MCEs from kernel messages to avoid bogus bug reports.
 */

static struct mce_log mcelog = {
	.signature	= MCE_LOG_SIGNATURE,
	.len		= MCE_LOG_LEN,
	.recordlen	= sizeof(struct mce),
    .lock       = SPIN_LOCK_UNLOCKED,
};

void mce_log(struct mce *mce)
{
	unsigned next, entry;

	mce->finished = 0;
	wmb();
    unsigned long flags;
    spin_lock_irqsave( &mcelog.lock, flags );
	for (;;) {
		entry = mcelog.next;
		for (;;) {

			/*
			 * When the buffer fills up discard new entries.
			 * Assume that the earlier errors are the more
			 * interesting ones:
			 */
			if (entry >= MCE_LOG_LEN) {
				set_bit(MCE_OVERFLOW,
					(unsigned long *)&mcelog.flags);
                spin_unlock_irqrestore( &mcelog.lock, flags );
				return;
			}
			/* Old left over entry. Skip: */
			if (mcelog.entry[entry].finished) {
				entry++;
				continue;
			}
			break;
		}
		smp_rmb();
		next = entry + 1;
		if (cmpxchg(&mcelog.next, entry, next) == entry)
			break;
	}
    spin_unlock_irqrestore( &mcelog.lock, flags );
	memcpy(mcelog.entry + entry, mce, sizeof(struct mce));
	wmb();
	mcelog.entry[entry].finished = 1;
	wmb();

	mce->finished = 1;
	set_bit(0, &mce_need_notify);
}

static void print_mce(struct mce *m)
{
	pr_emerg(HW_ERR "CPU %d: Machine Check Exception: %Lx Bank %d: %016Lx\n",
	       m->extcpu, m->mcgstatus, m->bank, m->status);

	if (m->ip) {
		pr_emerg(HW_ERR "RIP%s %02x:<%016Lx> ",
			!(m->mcgstatus & MCG_STATUS_EIPV) ? " !INEXACT!" : "",
				m->cs, m->ip);

		if (m->cs == __KERNEL_CS) {
            char buf[KSYM_SYMBOL_LEN];
			kallsyms_sprint_symbol(buf, m->ip);
			printk("{%s}", buf);
        }
		pr_cont("\n");
	}

	pr_emerg(HW_ERR "TSC %llx ", m->tsc);
	if (m->addr)
		pr_cont("ADDR %llx ", m->addr);
	if (m->misc)
		pr_cont("MISC %llx ", m->misc);

	pr_cont("\n");
	/*
	 * Note this output is parsed by external tools and old fields
	 * should not be changed.
	 */
	pr_emerg(HW_ERR "PROCESSOR %u:%x TIME %llu SOCKET %u APIC %x\n",
		m->cpuvendor, m->cpuid, m->time, m->socketid, m->apicid);

	pr_emerg(HW_ERR "Run the above through 'mcelog --ascii'\n");
}

#define PANIC_TIMEOUT 5 /* 5 seconds */

static atomic_t mce_paniced;

static int fake_panic;
static atomic_t mce_fake_paniced;

/* Panic in progress. Enable interrupts and wait for final IPI */
static void wait_for_panic(void)
{
	long timeout = PANIC_TIMEOUT*USEC_PER_SEC;

	local_irq_enable();
	while (timeout-- > 0)
		udelay(1);
	if (panic_timeout == 0)
		panic_timeout = mce_panic_timeout;
	panic("Panicing machine check CPU died");
}

static void mce_panic(char *msg, struct mce *final, char *exp)
{
	int i, apei_err = 0;

	if (!fake_panic) {
		/*
		 * Make sure only one CPU runs in machine check panic
		 */
		if (atomic_inc_return(&mce_paniced) > 1)
			wait_for_panic();
		barrier();
		bust_spinlocks(1);
	} else {
		/* Don't log too much for fake panic */
		if (atomic_inc_return(&mce_fake_paniced) > 1)
			return;
	}
	/* First print corrected ones that are still unlogged */
	for (i = 0; i < MCE_LOG_LEN; i++) {
		struct mce *m = &mcelog.entry[i];
		if (!(m->status & MCI_STATUS_VAL))
			continue;
		if (!(m->status & MCI_STATUS_UC)) {
			print_mce(m);
			if (!apei_err)
				apei_err = apei_write_mce(m);
		}
	}
	/* Now print uncorrected but with the final one last */
	for (i = 0; i < MCE_LOG_LEN; i++) {
		struct mce *m = &mcelog.entry[i];
		if (!(m->status & MCI_STATUS_VAL))
			continue;
		if (!(m->status & MCI_STATUS_UC))
			continue;
		if (!final || memcmp(m, final, sizeof(struct mce))) {
			print_mce(m);
			if (!apei_err)
				apei_err = apei_write_mce(m);
		}
	}
	if (final) {
		print_mce(final);
		if (!apei_err)
			apei_err = apei_write_mce(final);
	}
	if (cpu_missing)
		pr_emerg(HW_ERR "Some CPUs didn't answer in synchronization\n");
	if (exp)
		pr_emerg(HW_ERR "Machine check: %s\n", exp);
	if (!fake_panic) {
		if (panic_timeout == 0)
			panic_timeout = mce_panic_timeout;
		panic(msg);
	} else
		pr_emerg(HW_ERR "Fake kernel panic: %s\n", msg);
}

/* Support code for software error injection */

static int msr_to_offset(u32 msr)
{
    struct mce *i = &per_cpu(injectm, this_cpu);
    unsigned bank = i->bank;

	if (msr == rip_msr)
		return offsetof(struct mce, ip);
	if (msr == MSR_IA32_MCx_STATUS(bank))
		return offsetof(struct mce, status);
	if (msr == MSR_IA32_MCx_ADDR(bank))
		return offsetof(struct mce, addr);
	if (msr == MSR_IA32_MCx_MISC(bank))
		return offsetof(struct mce, misc);
	if (msr == MSR_IA32_MCG_STATUS)
		return offsetof(struct mce, mcgstatus);
	return -1;
}

/* MSR access wrappers used for error injection */
static u64 mce_rdmsrl(u32 msr)
{
	u64 v;
    struct mce *i = &per_cpu(injectm, this_cpu);

	if (i->finished) {
		int offset = msr_to_offset(msr);

		if (offset < 0)
			return 0;
		return *(u64 *)((char *)&__get_cpu_var(injectm) + offset);
	}

	if (rdmsrl_safe(msr, &v)) {
		WARN_ONCE(1, "mce: Unable to read msr %d!\n", msr);
		/*
		 * Return zero in case the access faulted. This should
		 * not happen normally but can happen if the CPU does
		 * something weird, or if the code is buggy.
		 */
		v = 0;
	}

	return v;
}

static void mce_wrmsrl(u32 msr, u64 v)
{
    struct mce *i = &per_cpu(injectm, this_cpu);

	if (i->finished) {
		int offset = msr_to_offset(msr);

		if (offset >= 0)
			*(u64 *)((char *)&__get_cpu_var(injectm) + offset) = v;
		return;
	}
	wrmsrl(msr, v);
}

/*
 * Collect all global (w.r.t. this processor) status about this machine
 * check into our "mce" struct so that we can use it later to assess
 * the severity of the problem as we read per-bank specific details.
 */
static inline void mce_gather_info(struct mce *m, struct pt_regs *regs)
{
	mce_setup(m);

	m->mcgstatus = mce_rdmsrl(MSR_IA32_MCG_STATUS);
	if (regs) {
		/*
		 * Get the address of the instruction at the time of
		 * the machine check error.
		 */
		if (m->mcgstatus & (MCG_STATUS_RIPV|MCG_STATUS_EIPV)) {
			m->ip = regs->rip;
			m->cs = regs->cs;
		}
		/* Use accurate RIP reporting if available. */
		if (rip_msr)
			m->ip = mce_rdmsrl(rip_msr);
	}
}

/*
 * Simple lockless ring to communicate PFNs from the exception handler with the
 * process context work function. This is vastly simplified because there's
 * only a single reader and a single writer.
 */
#define MCE_RING_SIZE 16	/* we use one entry less */

struct mce_ring {
	unsigned short start;
	unsigned short end;
	unsigned long ring[MCE_RING_SIZE];
};
static DEFINE_PER_CPU(struct mce_ring, mce_ring);

/* Runs with CPU affinity in workqueue */
static int mce_ring_empty(void)
{
	struct mce_ring *r = &__get_cpu_var(mce_ring);

	return r->start == r->end;
}

static int mce_ring_get(unsigned long *pfn)
{
	struct mce_ring *r;
	int ret = 0;

	*pfn = 0;
	get_cpu();
	r = &__get_cpu_var(mce_ring);
	if (r->start == r->end)
		goto out;
	*pfn = r->ring[r->start];
	r->start = (r->start + 1) % MCE_RING_SIZE;
	ret = 1;
out:
	put_cpu();
	return ret;
}

/* Always runs in MCE context with preempt off */
static int mce_ring_add(unsigned long pfn)
{
	struct mce_ring *r = &__get_cpu_var(mce_ring);
	unsigned next;

	next = (r->end + 1) % MCE_RING_SIZE;
	if (next == r->start)
		return -1;
	r->ring[r->end] = pfn;
	wmb();
	r->end = next;
	return 0;
}

int mce_available(struct cpuinfo_x86 *c)
{
	if (mce_disabled)
		return 0;
	return cpu_has(c, X86_FEATURE_MCE) && cpu_has(c, X86_FEATURE_MCA);
}

static void mce_schedule_work(void)
{
	if (!mce_ring_empty()) {
		struct work_struct *work = &__get_cpu_var(mce_work);
		if (!work_pending(work))
			schedule_work(work);
	}
}

DEFINE_PER_CPU(struct irq_work, mce_irq_work);

void mce_irq_work_cb(struct irq_work *entry)
{
	mce_notify_irq();
	mce_schedule_work();
}

static void mce_report_event(struct pt_regs *regs)
{
	if (regs->eflags & (X86_VM_MASK|X86_EFLAGS_IF)) {
		mce_notify_irq();
		/*
		 * Triggering the work queue here is just an insurance
		 * policy in case the syscall exit notify handler
		 * doesn't run soon enough or ends up running on the
		 * wrong CPU (can happen when audit sleeps)
		 */
		mce_schedule_work();
		return;
	}

	irq_work_queue(&__get_cpu_var(mce_irq_work));
}

/*
 * Poll for corrected events or events that happened before reset.
 * Those are just logged through /dev/mcelog.
 *
 * This is executed in standard interrupt context.
 *
 * Note: spec recommends to panic for fatal unsignalled
 * errors here. However this would be quite problematic --
 * we would need to reimplement the Monarch handling and
 * it would mess up the exclusion between exception handler
 * and poll hander -- * so we skip this for now.
 * These cases should not happen anyways, or only when the CPU
 * is already totally * confused. In this case it's likely it will
 * not fully execute the machine check handler either.
 */
void machine_check_poll(enum mcp_flags flags, mce_banks_t *b)
{
	struct mce m;
	int i;

	mce_gather_info(&m, NULL);

	for (i = 0; i < banks; i++) {
		if (!mce_banks[i].ctl || !test_bit(i, *b))
			continue;

		m.misc = 0;
		m.addr = 0;
		m.bank = i;
		m.tsc = 0;

		barrier();
		m.status = mce_rdmsrl(MSR_IA32_MCx_STATUS(i));
		if (!(m.status & MCI_STATUS_VAL))
			continue;

		/*
		 * Uncorrected or signalled events are handled by the exception
		 * handler when it is enabled, so don't process those here.
		 *
		 * TBD do the same check for MCI_STATUS_EN here?
		 */
		if (!(flags & MCP_UC) &&
		    (m.status & (mce_ser ? MCI_STATUS_S : MCI_STATUS_UC)))
			continue;

		if (m.status & MCI_STATUS_MISCV)
			m.misc = mce_rdmsrl(MSR_IA32_MCx_MISC(i));
		if (m.status & MCI_STATUS_ADDRV)
			m.addr = mce_rdmsrl(MSR_IA32_MCx_ADDR(i));

		if (!(flags & MCP_TIMESTAMP))
			m.tsc = 0;
		/*
		 * Don't get the IP here because it's unlikely to
		 * have anything to do with the actual error location.
		 */
		if (!(flags & MCP_DONTLOG) && !mce_dont_log_ce)
			mce_log(&m);

		/*
		 * Clear state for this bank.
		 */
		mce_wrmsrl(MSR_IA32_MCx_STATUS(i), 0);
	}

	/*
	 * Don't clear MCG_STATUS here because it's only defined for
	 * exceptions.
	 */

	sync_core();
}
EXPORT_SYMBOL_GPL(machine_check_poll);

/*
 * Do a quick check if any of the events requires a panic.
 * This decides if we keep the events around or clear them.
 */
static int mce_no_way_out(struct mce *m, char **msg)
{
	int i;

	for (i = 0; i < banks; i++) {
		m->status = mce_rdmsrl(MSR_IA32_MCx_STATUS(i));
		if (mce_severity(m, tolerant, msg) >= MCE_PANIC_SEVERITY)
			return 1;
	}
	return 0;
}

/*
 * Variable to establish order between CPUs while scanning.
 * Each CPU spins initially until executing is equal its number.
 */
static atomic_t mce_executing;

/*
 * Defines order of CPUs on entry. First CPU becomes Monarch.
 */
static atomic_t mce_callin;

/*
 * Check if a timeout waiting for other CPUs happened.
 */
static int mce_timed_out(u64 *t)
{
	/*
	 * The others already did panic for some reason.
	 * Bail out like in a timeout.
	 * rmb() to tell the compiler that system_state
	 * might have been modified by someone else.
	 */
	rmb();
	if (atomic_read(&mce_paniced))
		wait_for_panic();
	if (!monarch_timeout)
		goto out;
	if ((s64)*t < SPINUNIT) {
		/* CHECKME: Make panic default for 1 too? */
		if (tolerant < 1)
			mce_panic("Timeout synchronizing machine check over CPUs",
				  NULL, NULL);
		cpu_missing = 1;
		return 1;
	}
	*t -= SPINUNIT;
out:
	touch_nmi_watchdog();
	return 0;
}

/*
 * The Monarch's reign.  The Monarch is the CPU who entered
 * the machine check handler first. It waits for the others to
 * raise the exception too and then grades them. When any
 * error is fatal panic. Only then let the others continue.
 *
 * The other CPUs entering the MCE handler will be controlled by the
 * Monarch. They are called Subjects.
 *
 * This way we prevent any potential data corruption in a unrecoverable case
 * and also makes sure always all CPU's errors are examined.
 *
 * Also this detects the case of a machine check event coming from outer
 * space (not detected by any CPUs) In this case some external agent wants
 * us to shut down, so panic too.
 *
 * The other CPUs might still decide to panic if the handler happens
 * in a unrecoverable place, but in this case the system is in a semi-stable
 * state and won't corrupt anything by itself. It's ok to let the others
 * continue for a bit first.
 *
 * All the spin loops have timeouts; when a timeout happens a CPU
 * typically elects itself to be Monarch.
 */
static void mce_reign(void)
{
	int cpu;
	struct mce *m = NULL;
	int global_worst = 0;
	char *msg = NULL;
	char *nmsg = NULL;

	/*
	 * This CPU is the Monarch and the other CPUs have run
	 * through their handlers.
	 * Grade the severity of the errors of all the CPUs.
	 */
	for_each_present_cpu(cpu) {
		int severity = mce_severity(&per_cpu(mces_seen, cpu), tolerant,
					    &nmsg);
		if (severity > global_worst) {
			msg = nmsg;
			global_worst = severity;
			m = &per_cpu(mces_seen, cpu);
		}
	}

	/*
	 * Cannot recover? Panic here then.
	 * This dumps all the mces in the log buffer and stops the
	 * other CPUs.
	 */
	if (m && global_worst >= MCE_PANIC_SEVERITY && tolerant < 3)
		mce_panic("Fatal Machine check", m, msg);

	/*
	 * For UC somewhere we let the CPU who detects it handle it.
	 * Also must let continue the others, otherwise the handling
	 * CPU could deadlock on a lock.
	 */

	/*
	 * No machine check event found. Must be some external
	 * source or one CPU is hung. Panic.
	 */
	if (global_worst <= MCE_KEEP_SEVERITY && tolerant < 3)
		mce_panic("Machine check from unknown source", NULL, NULL);

	/*
	 * Now clear all the mces_seen so that they don't reappear on
	 * the next mce.
	 */
	for_each_present_cpu(cpu)
		memset(&per_cpu(mces_seen, cpu), 0, sizeof(struct mce));
}

static atomic_t global_nwo;

/*
 * Start of Monarch synchronization. This waits until all CPUs have
 * entered the exception handler and then determines if any of them
 * saw a fatal event that requires panic. Then it executes them
 * in the entry order.
 * TBD double check parallel CPU hotunplug
 */
static int mce_start(int *no_way_out)
{
	int order;
	int cpus = num_online_cpus();
	u64 timeout = (u64)monarch_timeout * NSEC_PER_USEC;

	if (!timeout)
		return -1;

	atomic_add(*no_way_out, &global_nwo);
	/*
	 * global_nwo should be updated before mce_callin
	 */
	smp_wmb();
	order = atomic_inc_return(&mce_callin);

	/*
	 * Wait for everyone.
	 */
	while (atomic_read(&mce_callin) != cpus) {
		if (mce_timed_out(&timeout)) {
			atomic_set(&global_nwo, 0);
			return -1;
		}
		ndelay(SPINUNIT);
	}

	/*
	 * mce_callin should be read before global_nwo
	 */
	smp_rmb();

	if (order == 1) {
		/*
		 * Monarch: Starts executing now, the others wait.
		 */
		atomic_set(&mce_executing, 1);
	} else {
		/*
		 * Subject: Now start the scanning loop one by one in
		 * the original callin order.
		 * This way when there are any shared banks it will be
		 * only seen by one CPU before cleared, avoiding duplicates.
		 */
		while (atomic_read(&mce_executing) < order) {
			if (mce_timed_out(&timeout)) {
				atomic_set(&global_nwo, 0);
				return -1;
			}
			ndelay(SPINUNIT);
		}
	}

	/*
	 * Cache the global no_way_out state.
	 */
	*no_way_out = atomic_read(&global_nwo);

	return order;
}

/*
 * Synchronize between CPUs after main scanning loop.
 * This invokes the bulk of the Monarch processing.
 */
static int mce_end(int order)
{
	int ret = -1;
	u64 timeout = (u64)monarch_timeout * NSEC_PER_USEC;

	if (!timeout)
		goto reset;
	if (order < 0)
		goto reset;

	/*
	 * Allow others to run.
	 */
	atomic_inc(&mce_executing);

	if (order == 1) {
		/* CHECKME: Can this race with a parallel hotplug? */
		int cpus = num_online_cpus();

		/*
		 * Monarch: Wait for everyone to go through their scanning
		 * loops.
		 */
		while (atomic_read(&mce_executing) <= cpus) {
			if (mce_timed_out(&timeout))
				goto reset;
			ndelay(SPINUNIT);
		}

		mce_reign();
		barrier();
		ret = 0;
	} else {
		/*
		 * Subject: Wait for Monarch to finish.
		 */
		while (atomic_read(&mce_executing) != 0) {
			if (mce_timed_out(&timeout))
				goto reset;
			ndelay(SPINUNIT);
		}

		/*
		 * Don't reset anything. That's done by the Monarch.
		 */
		return 0;
	}

	/*
	 * Reset all global state.
	 */
reset:
	atomic_set(&global_nwo, 0);
	atomic_set(&mce_callin, 0);
	barrier();

	/*
	 * Let others run again.
	 */
	atomic_set(&mce_executing, 0);
	return ret;
}

/*
 * Check if the address reported by the CPU is in a format we can parse.
 * It would be possible to add code for most other cases, but all would
 * be somewhat complicated (e.g. segment offset would require an instruction
 * parser). So only support physical addresses up to page granuality for now.
 */
static int mce_usable_address(struct mce *m)
{
	if (!(m->status & MCI_STATUS_MISCV) || !(m->status & MCI_STATUS_ADDRV))
		return 0;
	if (MCI_MISC_ADDR_LSB(m->misc) > PAGE_SHIFT)
		return 0;
	if (MCI_MISC_ADDR_MODE(m->misc) != MCI_MISC_ADDR_PHYS)
		return 0;
	return 1;
}

static void mce_clear_state(unsigned long *toclear)
{
	int i;

	for (i = 0; i < banks; i++) {
		if (test_bit(i, toclear))
			mce_wrmsrl(MSR_IA32_MCx_STATUS(i), 0);
	}
}

/*
 * The actual machine check handler. This only handles real
 * exceptions when something got corrupted coming in through int 18.
 *
 * This is executed in NMI context not subject to normal locking rules. This
 * implies that most kernel services cannot be safely used. Don't even
 * think about putting a printk in there!
 *
 * On Intel systems this is entered on all CPUs in parallel through
 * MCE broadcast. However some CPUs might be broken beyond repair,
 * so be always careful when synchronizing with others.
 */
void do_machine_check(struct pt_regs *regs, long error_code)
{
	struct mce m, *final;
	int i;
	int worst = 0;
	int severity;
	/*
	 * Establish sequential order between the CPUs entering the machine
	 * check handler.
	 */
	int order;
	/*
	 * If no_way_out gets set, there is no safe way to recover from this
	 * MCE.  If tolerant is cranked up, we'll try anyway.
	 */
	int no_way_out = 0;
	/*
	 * If kill_it gets set, there might be a way to recover from this
	 * error.
	 */
	int kill_it = 0;
	DECLARE_BITMAP(toclear, MAX_NR_BANKS);
	char *msg = "Unknown";

	atomic_inc(&mce_entry);

	if (!banks)
		goto out;

	mce_gather_info(&m, regs);

	final = &__get_cpu_var(mces_seen);
	*final = m;

	no_way_out = mce_no_way_out(&m, &msg);

	barrier();

	/*
	 * When no restart IP must always kill or panic.
	 */
	if (!(m.mcgstatus & MCG_STATUS_RIPV))
		kill_it = 1;

	/*
	 * Go through all the banks in exclusion of the other CPUs.
	 * This way we don't report duplicated events on shared banks
	 * because the first one to see it will clear it.
	 */
	order = mce_start(&no_way_out);
	for (i = 0; i < banks; i++) {
		__clear_bit(i, toclear);
		if (!mce_banks[i].ctl)
			continue;

		m.misc = 0;
		m.addr = 0;
		m.bank = i;

		m.status = mce_rdmsrl(MSR_IA32_MCx_STATUS(i));
		if ((m.status & MCI_STATUS_VAL) == 0)
			continue;

		/*
		 * Non uncorrected or non signaled errors are handled by
		 * machine_check_poll. Leave them alone, unless this panics.
		 */
		if (!(m.status & (mce_ser ? MCI_STATUS_S : MCI_STATUS_UC)) &&
			!no_way_out)
			continue;

		/*
		 * Set taint even when machine check was not enabled.
		 */
		add_taint(TAINT_MACHINE_CHECK);

		severity = mce_severity(&m, tolerant, NULL);

		/*
		 * When machine check was for corrected handler don't touch,
		 * unless we're panicing.
		 */
		if (severity == MCE_KEEP_SEVERITY && !no_way_out)
			continue;
		__set_bit(i, toclear);
		if (severity == MCE_NO_SEVERITY) {
			/*
			 * Machine check event was not enabled. Clear, but
			 * ignore.
			 */
			continue;
		}

		/*
		 * Kill on action required.
		 */
		if (severity == MCE_AR_SEVERITY)
			kill_it = 1;

		if (m.status & MCI_STATUS_MISCV)
			m.misc = mce_rdmsrl(MSR_IA32_MCx_MISC(i));
		if (m.status & MCI_STATUS_ADDRV)
			m.addr = mce_rdmsrl(MSR_IA32_MCx_ADDR(i));

		/*
		 * Action optional error. Queue address for later processing.
		 * When the ring overflows we just ignore the AO error.
		 * RED-PEN add some logging mechanism when
		 * usable_address or mce_add_ring fails.
		 * RED-PEN don't ignore overflow for tolerant == 0
		 */
		if (severity == MCE_AO_SEVERITY && mce_usable_address(&m))
			mce_ring_add(m.addr >> PAGE_SHIFT);

		mce_log(&m);

		if (severity > worst) {
			*final = m;
			worst = severity;
		}
	}

	if (!no_way_out)
		mce_clear_state(toclear);

	/*
	 * Do most of the synchronization with other CPUs.
	 * When there's any problem use only local no_way_out state.
	 */
	if (mce_end(order) < 0)
		no_way_out = worst >= MCE_PANIC_SEVERITY;

	/*
	 * If we have decided that we just CAN'T continue, and the user
	 * has not set tolerant to an insane level, give up and die.
	 *
	 * This is mainly used in the case when the system doesn't
	 * support MCE broadcasting or it has been disabled.
	 */
	if (no_way_out && tolerant < 3)
		mce_panic("Fatal machine check on current CPU", final, msg);

	/*
	 * If the error seems to be unrecoverable, something should be
	 * done.  Try to kill as little as possible.  If we can kill just
	 * one task, do that.  If the user has set the tolerance very
	 * high, don't try to do anything at all.
	 */

	if (kill_it && tolerant < 3)
		force_sig(SIGBUS, current);

	/* notify userspace ASAP */
	set_thread_flag(TIF_MCE_NOTIFY);

	if (worst > 0)
		mce_report_event(regs);
	mce_wrmsrl(MSR_IA32_MCG_STATUS, 0);
out:
	atomic_dec(&mce_entry);
	sync_core();
}
EXPORT_SYMBOL_GPL(do_machine_check);

/* dummy to break dependency. actual code is in mm/memory-failure.c */
void __attribute__((weak)) memory_failure(unsigned long pfn, int vector)
{
	printk(KERN_ERR "Action optional memory failure at %lx ignored\n", pfn);
}

/*
 * Called after mce notification in process context. This code
 * is allowed to sleep. Call the high level VM handler to process
 * any corrupted pages.
 * Assume that the work queue code only calls this one at a time
 * per CPU.
 * Note we don't disable preemption, so this code might run on the wrong
 * CPU. In this case the event is picked up by the scheduled work queue.
 * This is merely a fast path to expedite processing in some common
 * cases.
 */
void mce_notify_process(void)
{
	unsigned long pfn;
	mce_notify_irq();
	while (mce_ring_get(&pfn))
		memory_failure(pfn, MACHINE_CHECK_VECTOR);
}

static void mce_process_work(struct work_struct *dummy)
{
	mce_notify_process();
}

#ifdef CONFIG_X86_MCE_INTEL
/***
 * mce_log_therm_throt_event - Logs the thermal throttling event to mcelog
 * @cpu: The CPU on which the event occurred.
 * @status: Event status information
 *
 * This function should be called by the thermal interrupt after the
 * event has been processed and the decision was made to log the event
 * further.
 *
 * The status parameter will be saved to the 'status' field of 'struct mce'
 * and historically has been the register value of the
 * MSR_IA32_THERMAL_STATUS (Intel) msr.
 */
void mce_log_therm_throt_event(__u64 status)
{
	struct mce m;

	mce_setup(&m);
	m.bank = MCE_THERMAL_BANK;
	m.status = status;
	mce_log(&m);
}
#endif /* CONFIG_X86_MCE_INTEL */

static int check_interval = 1 * 60; /* 1 minutes */

static DEFINE_PER_CPU(unsigned long, mce_next_interval); /* in nanoseconds */
static DEFINE_PER_CPU(struct timer, mce_timer);

static void mce_start_timer(unsigned long data)
{
	struct timer *t = &per_cpu(mce_timer, data);

	WARN_ON(smp_processor_id() != data);

	if (mce_available(__this_cpu_ptr(&cpu_info))) {
		machine_check_poll(MCP_TIMESTAMP,
				&__get_cpu_var(mce_poll_banks));
	}

    mce_notify_irq();

	t->expires = get_time() + __get_cpu_var(mce_next_interval);
	timer_add(t);
}

/*
 * Notify the user(s) about new machine check events.
 * Can be called from interrupt context, but not from machine check/NMI
 * context.
 */
int mce_notify_irq(void)
{
	clear_thread_flag(TIF_MCE_NOTIFY);

	if (test_and_clear_bit(0, &mce_need_notify)) {
		/* wake processes polling /dev/mcelog */
		waitq_wake_nr(&mce_chrdev_wait,1);

		pr_info(HW_ERR "Machine check events logged\n");

		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mce_notify_irq);

static int __cpuinit __mcheck_cpu_mce_banks_init(void)
{
	int i;

	mce_banks = alloc_bootmem_aligned( banks * sizeof(struct mce_bank) , 
                PAGE_SIZE ); 
	if (!mce_banks)
		return -ENOMEM;
	for (i = 0; i < banks; i++) {
		struct mce_bank *b = &mce_banks[i];

		b->ctl = -1ULL;
		b->init = 1;
	}
	return 0;
}

/*
 * Initialize Machine Checks for a CPU.
 */
static int __cpuinit __mcheck_cpu_cap_init(void)
{
	unsigned b;
	u64 cap;

	rdmsrl(MSR_IA32_MCG_CAP, cap);

	b = cap & MCG_BANKCNT_MASK;
	if (!banks)
		printk(KERN_INFO "mce: CPU supports %d MCE banks\n", b);

	if (banks == 0)
		return -1;

	if (b > MAX_NR_BANKS) {
		printk(KERN_WARNING
		       "MCE: Using only %u machine check banks out of %u\n",
			MAX_NR_BANKS, b);
		b = MAX_NR_BANKS;
	}

	/* Don't support asymmetric configurations today */
	WARN_ON(banks != 0 && b != banks);
	banks = b;
	if (!mce_banks) {
		int err = __mcheck_cpu_mce_banks_init();

		if (err)
			return err;
	}

	/* Use accurate RIP reporting if available. */
	if ((cap & MCG_EXT_P) && MCG_EXT_CNT(cap) >= 9)
		rip_msr = MSR_IA32_MCG_EIP;

	if (cap & MCG_SER_P)
		mce_ser = 1;

	return 0;
}

static void __mcheck_cpu_init_generic(void)
{
	mce_banks_t all_banks;
	u64 cap;
	int i;

	/*
	 * Log the machine checks left over from the previous reset.
	 */
	bitmap_fill(all_banks, MAX_NR_BANKS);
	machine_check_poll(MCP_UC|(!mce_bootlog ? MCP_DONTLOG : 0), &all_banks);

	set_in_cr4(X86_CR4_MCE);

	rdmsrl(MSR_IA32_MCG_CAP, cap);
	if (cap & MCG_CTL_P)
		wrmsr(MSR_IA32_MCG_CTL, 0xffffffff, 0xffffffff);

	for (i = 0; i < banks; i++) {
		struct mce_bank *b = &mce_banks[i];

		if (!b->init)
			continue;
		wrmsrl(MSR_IA32_MCx_CTL(i), b->ctl);
		wrmsrl(MSR_IA32_MCx_STATUS(i), 0);
	}
}

/* Add per CPU specific workarounds here */
static int __cpuinit __mcheck_cpu_apply_quirks(struct cpuinfo_x86 *c)
{
	if (c->arch.x86_vendor == X86_VENDOR_UNKNOWN) {
		pr_info("MCE: unknown CPU type - not enabling MCE support.\n");
		return -EOPNOTSUPP;
	}

	/* This should be disabled by the BIOS, but isn't always */
	if (c->arch.x86_vendor == X86_VENDOR_AMD) {
		if (c->arch.x86_family == 15 && banks > 4) {
			/*
			 * disable GART TBL walk error reporting, which
			 * trips off incorrectly with the IOMMU & 3ware
			 * & Cerberus:
			 */
			clear_bit(10, (unsigned long *)&mce_banks[4].ctl);
		}
		if (c->arch.x86_family <= 17 && mce_bootlog < 0) {
			/*
			 * Lots of broken BIOS around that don't clear them
			 * by default and leave crap in there. Don't log:
			 */
			mce_bootlog = 0;
		}
		/*
		 * Various K7s with broken bank 0 around. Always disable
		 * by default.
		 */
		 if (c->arch.x86_family == 6 && banks > 0)
			mce_banks[0].ctl = 0;
	}

	if (c->arch.x86_vendor == X86_VENDOR_INTEL) {
		/*
		 * SDM documents that on family 6 bank 0 should not be written
		 * because it aliases to another special BIOS controlled
		 * register.
		 * But it's not aliased anymore on model 0x1a+
		 * Don't ignore bank 0 completely because there could be a
		 * valid event later, merely don't write CTL0.
		 */

		if (c->arch.x86_family == 6 && c->arch.x86_model < 0x1A && banks > 0)
			mce_banks[0].init = 0;

		/*
		 * All newer Intel systems support MCE broadcasting. Enable
		 * synchronization with a one second timeout.
		 */
		if ((c->arch.x86_family > 6 || 
                    (c->arch.x86_family == 6 && c->arch.x86_model >= 0xe)) &&
			monarch_timeout < 0)
			monarch_timeout = USEC_PER_SEC;

		/*
		 * There are also broken BIOSes on some Pentium M and
		 * earlier systems:
		 */
		if (c->arch.x86_family == 6 && c->arch.x86_model <= 13 && mce_bootlog < 0)
			mce_bootlog = 0;
	}
	if (monarch_timeout < 0)
		monarch_timeout = 0;
	if (mce_bootlog != 0)
		mce_panic_timeout = 30;

	return 0;
}

/*
 * AMD CPUs have a feature that allows MCA MSRs to be set for debugging.
 * sys_mce_inject() allows a user application to trigger the machine check
 * interrupt.
 */
int sys_mce_inject( unsigned long mcg_status,
                        int bank,
                        unsigned long status,
                        unsigned long addr,
                        unsigned long misc )
{
    wrmsrl( MSR_IA32_MCG_STATUS, mcg_status );
    wrmsrl( MSR_IA32_MCx_STATUS( bank ), status );
    wrmsrl( MSR_IA32_MCx_MISC( bank ), misc );
    wrmsrl( MSR_IA32_MCx_ADDR( bank ), addr );

    asm("int $18\n");

    return 0;
}


static void __mcheck_cpu_init_vendor(struct cpuinfo_x86 *c)
{
	switch (c->arch.x86_vendor) {
	case X86_VENDOR_INTEL:
		mce_intel_feature_init(c);
		break;
	case X86_VENDOR_AMD:

        { 
            /*
             * Setup user triggerable machine check interrupts
             */
            unsigned long value;
            rdmsrl(MSR_K8_HWCR, value);
            value |= 1 << 18;
            wrmsrl(MSR_K8_HWCR, value);
            syscall_register( __NR_mce_inject, (syscall_ptr_t) sys_mce_inject );
        }

		mce_amd_feature_init(c);
		break;
	default:
		break;
	}
}

static void __mcheck_cpu_init_timer(void)
{
	struct timer *t = &__get_cpu_var(mce_timer);
	unsigned long *n = &__get_cpu_var(mce_next_interval);

    t->function = mce_start_timer;
    t->data = this_cpu;

	if (mce_ignore_ce)
		return;

	*n = check_interval * NSEC_PER_SEC;
	if (!*n)
		return;
	t->expires = get_time() + *n;
	timer_add(t);
}

/* Handle unconfigured int18 (should never happen) */
static void unexpected_machine_check(struct pt_regs *regs, long error_code)
{
	printk(KERN_ERR "CPU#%d: Unexpected int18 (Machine Check).\n",
	       smp_processor_id());
}

/* Call the installed machine check handler for this CPU setup. */
void (*machine_check_vector)(struct pt_regs *, long error_code) =
						unexpected_machine_check;

/*
 * Called for each booted CPU to set up machine checks.
 * Must be called with preempt off:
 */
void __cpuinit mcheck_cpu_init(void)
{
    struct cpuinfo *c = &cpu_info[this_cpu];
	if (mce_disabled)
		return;


	if (!mce_available(c))
		return;

	if (__mcheck_cpu_cap_init() < 0 || __mcheck_cpu_apply_quirks(c) < 0) {
		mce_disabled = 1;
		return;
	}

	machine_check_vector = do_machine_check;

	__mcheck_cpu_init_generic();
	__mcheck_cpu_init_vendor(c);
}

static void __mcheck_cpu_init_late(void* unused)
{
	__mcheck_cpu_init_timer();
	INIT_WORK(&__get_cpu_var(mce_work), mce_process_work);
	init_irq_work(&__get_cpu_var(mce_irq_work), &mce_irq_work_cb);
}

void mcheck_init_late(void)
{
    xcall_function( cpu_present_map, __mcheck_cpu_init_late, NULL, 1 );
}

/*
 * mce_chrdev: Character device /dev/mcelog to read and clear the MCE log.
 */

static DEFINE_SPINLOCK(mce_chrdev_state_lock);
static int mce_chrdev_open_count;	/* #times opened */
static int mce_chrdev_open_exclu;	/* already open exclusive? */

static int mce_chrdev_open(struct inode *inode, struct file *file)
{
    return 0;
#if 0 // mce_chrdev_open  
	spin_lock(&mce_chrdev_state_lock);

	if (mce_chrdev_open_exclu ||
	    (mce_chrdev_open_count && (file->f_flags & O_EXCL))) {
		spin_unlock(&mce_chrdev_state_lock);

		return -EBUSY;
	}

	if (file->f_flags & O_EXCL)
		mce_chrdev_open_exclu = 1;
	mce_chrdev_open_count++;

	spin_unlock(&mce_chrdev_state_lock);

	return nonseekable_open(inode, file);
#endif
}

static int mce_chrdev_release(struct inode *inode, struct file *file)
{
	spin_lock(&mce_chrdev_state_lock);

	mce_chrdev_open_count--;
	mce_chrdev_open_exclu = 0;

	spin_unlock(&mce_chrdev_state_lock);

	return 0;
}

static void collect_tscs(void *data)
{
	unsigned long *cpu_tsc = (unsigned long *)data;

	rdtscll(cpu_tsc[smp_processor_id()]);
}

static int mce_apei_read_done;

/* Collect MCE record of previous boot in persistent storage via APEI ERST. */
static int __mce_read_apei(char __user **ubuf, size_t usize)
{
	int rc;
	u64 record_id;
	struct mce m;

	if (usize < sizeof(struct mce))
		return -EINVAL;

	rc = apei_read_mce(&m, &record_id);
	/* Error or no more MCE record */
	if (rc <= 0) {
		mce_apei_read_done = 1;
		return rc;
	}
	rc = -EFAULT;
	if (copy_to_user(*ubuf, &m, sizeof(struct mce)))
		return rc;
	/*
	 * In fact, we should have cleared the record after that has
	 * been flushed to the disk or sent to network in
	 * /sbin/mcelog, but we have no interface to support that now,
	 * so just clear it to avoid duplication.
	 */
	rc = apei_clear_mce(record_id);
	if (rc) {
		mce_apei_read_done = 1;
		return rc;
	}
	*ubuf += sizeof(struct mce);

	return 0;
}

static ssize_t mce_chrdev_read(struct file *filp, char __user *ubuf,
				size_t usize, loff_t *off)
{
	char __user *buf = ubuf;
	unsigned long *cpu_tsc;
	unsigned prev, next;
	int i, err;

	cpu_tsc = kmem_alloc(NR_CPUS * sizeof(long));
	if (!cpu_tsc)
		return -ENOMEM;

	mutex_lock(&mce_chrdev_read_mutex);

	if (!mce_apei_read_done) {
		err = __mce_read_apei(&buf, usize);
		if (err || buf != ubuf)
			goto out;
	}

	/* Only supports full reads right now */
	err = -EINVAL;
	if ( usize < MCE_LOG_LEN*sizeof(struct mce))
		goto out;

    unsigned long flags;
    spin_lock_irqsave( &mcelog.lock, flags );
	next = mcelog.next;

	err = 0;
	prev = 0;
	do {
		for (i = prev; i < next; i++) {
			unsigned long timeout_time = get_time() +  5 * NSEC_PER_MSEC;
			struct mce *m = &mcelog.entry[i];

			while (!m->finished) {
				if ( get_time() >= timeout_time ) {
					memset(m, 0, sizeof(*m));
                    spin_unlock_irqrestore( &mcelog.lock, flags );
					goto timeout;
				}
				cpu_relax();
			}
			smp_rmb();
			err |= copy_to_user(buf, m, sizeof(*m));
			buf += sizeof(*m);
timeout:
			;
		}

		memset(mcelog.entry + prev, 0,
		       (next - prev) * sizeof(struct mce));
		prev = next;
		next = cmpxchg(&mcelog.next, prev, 0);
	} while (next != prev);

    spin_unlock_irqrestore( &mcelog.lock, flags );

    synchronize_sched();
	/*
	 * Collect entries that were still getting written before the
	 * synchronize.
	 */
	on_each_cpu(collect_tscs, cpu_tsc, 1);

	for (i = next; i < MCE_LOG_LEN; i++) {
		struct mce *m = &mcelog.entry[i];

		if (m->finished && m->tsc < cpu_tsc[m->cpu]) {
			err |= copy_to_user(buf, m, sizeof(*m));
			smp_rmb();
			buf += sizeof(*m);
			memset(m, 0, sizeof(*m));
		}
	}

	if (err)
		err = -EFAULT;

out:
	mutex_unlock(&mce_chrdev_read_mutex);
	kmem_free(cpu_tsc);

	return err ? err : buf - ubuf;
}

static unsigned int mce_chrdev_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &mce_chrdev_wait, wait);
	if (mcelog.next)
		return POLLIN | POLLRDNORM;
	if (!mce_apei_read_done && apei_check_mce())
		return POLLIN | POLLRDNORM;
	return 0;
}

static long mce_chrdev_ioctl(struct file *f, unsigned int cmd,
				unsigned long arg)
{
	int __user *p = (int __user *)arg;

	if (current->uid != 0)
		return -EPERM;

	switch (cmd) {
	case MCE_GET_RECORD_LEN:
		return put_user(sizeof(struct mce), p);
	case MCE_GET_LOG_LEN:
		return put_user(MCE_LOG_LEN, p);
	case MCE_GETCLEAR_FLAGS: {
		unsigned flags;

		do {
			flags = mcelog.flags;
		} while (cmpxchg(&mcelog.flags, flags, 0) != flags);

		return put_user(flags, p);
	}
	default:
		return -ENOTTY;
	}
}

static ssize_t (*mce_write)(struct file *filp, const char __user *ubuf,
			    size_t usize, loff_t *off);

void register_mce_write_callback(ssize_t (*fn)(struct file *filp,
			     const char __user *ubuf,
			     size_t usize, loff_t *off))
{
	mce_write = fn;
}
EXPORT_SYMBOL_GPL(register_mce_write_callback);

ssize_t mce_chrdev_write(struct file *filp, const char __user *ubuf,
			 size_t usize, loff_t *off)
{
	if (mce_write)
		return mce_write(filp, ubuf, usize, off);
	else
		return -EINVAL;
}

static const struct kfs_fops mce_chrdev_ops = {
	.open			= mce_chrdev_open,
	.release		= mce_chrdev_release,
	.read			= mce_chrdev_read,
	.write			= mce_chrdev_write,
	.poll			= mce_chrdev_poll,
	.unlocked_ioctl		= mce_chrdev_ioctl,
};

int __init mcheck_init(void)
{
	mcheck_intel_therm_init();

    driver_init_by_name( "mce", "*" );
	return 0;
}

int  mcheck_dev_init(void) {
    struct inode *inode;
    inode = kfs_create("/dev/mcelog",NULL,&mce_chrdev_ops,S_IFCHR|0666,NULL,0);  
    if (!inode)
        panic("Failed to create /dev/zero.");
    return 0;
}

DRIVER_INIT( "kfs", mcheck_dev_init );

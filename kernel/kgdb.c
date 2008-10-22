/*
 * KGDB stub.
 *
 * Maintainer: Jason Wessel <jason.wessel@windriver.com>
 *
 * Copyright (C) 2000-2001 VERITAS Software Corporation.
 * Copyright (C) 2002-2004 Timesys Corporation
 * Copyright (C) 2003-2004 Amit S. Kale <amitkale@linsyssoft.com>
 * Copyright (C) 2004 Pavel Machek <pavel@suse.cz>
 * Copyright (C) 2004-2006 Tom Rini <trini@kernel.crashing.org>
 * Copyright (C) 2004-2006 LinSysSoft Technologies Pvt. Ltd.
 * Copyright (C) 2005-2008 Wind River Systems, Inc.
 * Copyright (C) 2007 MontaVista Software, Inc.
 * Copyright (C) 2008 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 * Copyright (C) 2008 Patrick Bridges, University of New Mexico
 *
 * Contributors at various stages not listed above:
 *  Jason Wessel ( jason.wessel@windriver.com )
 *  George Anzinger <george@mvista.com>
 *  Anurekh Saxena (anurekh.saxena@timesys.com)
 *  Lake Stevens Instrument Division (Glenn Engel)
 *  Jim Kingdon, Cygnus Support.
 *
 * Original KGDB stub: David Grothe <dave@gcom.com>,
 * Tigran Aivazian <tigran@sco.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */
#include <lwk/errno.h>
#include <lwk/compiler.h>
#include <lwk/interrupt.h>
#include <lwk/spinlock.h>
#include <lwk/console.h>
#include <lwk/kernel.h>
#include <lwk/ptrace.h>
#include <lwk/string.h>
#include <lwk/delay.h>
#include <lwk/init.h>
#include <lwk/aspace.h>
#include <lwk/kgdb.h>
#include <lwk/smp.h>
#include <lwk/params.h>
#include <lwk/linux_compat.h>

#include <arch/byteorder.h>
#include <arch/atomic.h>
#include <arch/cacheflush.h>
#include <arch/system.h>
#include <arch/uaccess.h>

static int kgdb_break_asap;

struct kgdb_state {
	int			ex_vector;
	int			signo;
	int			err_code;
	int			cpu;
	int			pass_exception;
	long			threadid;
	long			kgdb_usethreadid;
	struct pt_regs		*lwk_regs;
};

static struct debuggerinfo_struct {
	void			*debuggerinfo;
	struct task_struct	*task;
} kgdb_info[NR_CPUS];

/**
 * kgdb_connected - Is a host GDB connected to us?
 */
int				kgdb_connected;
EXPORT_SYMBOL_GPL(kgdb_connected);

/* All the KGDB handlers are installed */
static int			kgdb_io_module_registered;

/* Guard for recursive entry */
static int			exception_level;

static struct kgdb_io		*kgdb_io_ops;
static DEFINE_SPINLOCK(kgdb_registration_lock);

/* kgdb console driver is loaded */
static int kgdb_con_registered;
/* determine if kgdb console output should be used */
static int kgdb_use_con;

static int __init opt_kgdb_con(char *str)
{
	kgdb_use_con = 1;
	return 0;
}

param_func(kgdbcon, (param_set_fn)opt_kgdb_con);

/*
 * Holds information about breakpoints in a kernel. These breakpoints are
 * added and removed by gdb.
 */
static struct kgdb_bkpt		kgdb_break[KGDB_MAX_BREAKPOINTS] = {
	[0 ... KGDB_MAX_BREAKPOINTS-1] = { .state = BP_UNDEFINED }
};

/*
 * The CPU# of the active CPU, or -1 if none:
 */
atomic_t			kgdb_active = ATOMIC_INIT(-1);

/*
 * We use NR_CPUs not PERCPU, in case kgdb is used to debug early
 * bootup code (which might not have percpu set up yet):
 */
static atomic_t			passive_cpu_wait[NR_CPUS];
static atomic_t			cpu_in_kgdb[NR_CPUS];
atomic_t			kgdb_setting_breakpoint;

struct task_struct		*kgdb_usethread;
struct task_struct		*kgdb_contthread;

int				kgdb_single_step;

/* Our I/O buffers. */
static char			remcom_in_buffer[BUFMAX];
static char			remcom_out_buffer[BUFMAX];

/* Storage for the registers, in GDB format. */
static unsigned long		gdb_regs[(NUMREGBYTES +
					sizeof(unsigned long) - 1) /
					sizeof(unsigned long)];

/* to keep track of the CPU which is doing the single stepping*/
atomic_t			kgdb_cpu_doing_single_step = ATOMIC_INIT(-1);

/*
 * Finally, some KGDB code :-)
 */

/*
 * Weak aliases for breakpoint management,
 * can be overriden by architectures when needed:
 */
int __weak kgdb_validate_break_address(unsigned long addr)
{
	char tmp_variable[BREAK_INSTR_SIZE];

	return probe_kernel_read(tmp_variable, (char *)addr, BREAK_INSTR_SIZE);
}

int __weak kgdb_arch_set_breakpoint(unsigned long addr, char *saved_instr)
{
	int err;

	err = probe_kernel_read(saved_instr, (char *)addr, BREAK_INSTR_SIZE);
	if (err)
		return err;

	return probe_kernel_write((char *)addr, arch_kgdb_ops.gdb_bpt_instr,
				  BREAK_INSTR_SIZE);
}

int __weak kgdb_arch_remove_breakpoint(unsigned long addr, char *bundle)
{
	return probe_kernel_write((char *)addr,
				  (char *)bundle, BREAK_INSTR_SIZE);
}

unsigned long __weak kgdb_arch_pc(int exception, struct pt_regs *regs)
{
	return instruction_pointer(regs);
}

int __weak kgdb_arch_init(void)
{
	return 0;
}

/**
 *	kgdb_disable_hw_debug - Disable hardware debugging while we in kgdb.
 *	@regs: Current &struct pt_regs.
 *
 *	This function will be called if the particular architecture must
 *	disable hardware debugging while it is processing gdb packets or
 *	handling exception.
 */
void __weak kgdb_disable_hw_debug(struct pt_regs *regs)
{
}

/*
 * GDB remote protocol parser:
 */

static const char	hexchars[] = "0123456789abcdef";

static int hex(char ch)
{
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	if ((ch >= 'A') && (ch <= 'F'))
		return ch - 'A' + 10;
	return -1;
}

/* scan for the sequence $<data>#<checksum> */
static void get_packet(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int count;
	char ch;

	do {
		/*
		 * Spin and wait around for the start character, ignore all
		 * other characters:
		 */
		while ((ch = (kgdb_io_ops->read_char())) != '$')
			/* nothing */;

		kgdb_connected = 1;
		checksum = 0;
		xmitcsum = -1;

		count = 0;

		/*
		 * now, read until a # or end of buffer is found:
		 */
		while (count < (BUFMAX - 1)) {
			ch = kgdb_io_ops->read_char();
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}
		buffer[count] = 0;
		if (ch == '#') {
			xmitcsum = hex(kgdb_io_ops->read_char()) << 4;
			xmitcsum += hex(kgdb_io_ops->read_char());

			if (checksum != xmitcsum) {
				/* failed checksum */
				kgdb_io_ops->write_char('-');
			} else {
				/* successful transfer */
				kgdb_io_ops->write_char('+');
			}
			if (kgdb_io_ops->flush)
				kgdb_io_ops->flush();
		}
	} while (checksum != xmitcsum);
}

/*
 * Send the packet in buffer.
 * Check for gdb connection if asked for.
 */
static void put_packet(char *buffer)
{
	unsigned char checksum;
	int count;
	char ch;

	/*
	 * $<packet info>#<checksum>.
	 */
	while (1) {
		kgdb_io_ops->write_char('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count])) {
			kgdb_io_ops->write_char(ch);
			checksum += ch;
			count++;
		}

		kgdb_io_ops->write_char('#');
		kgdb_io_ops->write_char(hexchars[checksum >> 4]);
		kgdb_io_ops->write_char(hexchars[checksum & 0xf]);
		if (kgdb_io_ops->flush)
			kgdb_io_ops->flush();

		/* Now see what we get in reply. */
		ch = kgdb_io_ops->read_char();

		if (ch == 3)
			ch = kgdb_io_ops->read_char();

		/* If we get an ACK, we are done. */
		if (ch == '+')
			return;

		/*
		 * If we get the start of another packet, this means
		 * that GDB is attempting to reconnect.  We will NAK
		 * the packet being sent, and stop trying to send this
		 * packet.
		 */
		if (ch == '$') {
			kgdb_io_ops->write_char('-');
			if (kgdb_io_ops->flush)
				kgdb_io_ops->flush();
			return;
		}
	}
}

static char *pack_hex_byte(char *pkt, u8 byte)
{
	*pkt++ = hexchars[byte >> 4];
	*pkt++ = hexchars[byte & 0xf];

	return pkt;
}

/*
 * Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null). May return an error.
 */
int kgdb_mem2hex(char *mem, char *buf, int count)
{
	char *tmp;
	int err;

	/*
	 * We use the upper half of buf as an intermediate buffer for the
	 * raw memory copy.  Hex conversion will work against this one.
	 */
	tmp = buf + count;

	err = probe_kernel_read(tmp, mem, count);
	if (!err) {
		while (count > 0) {
			buf = pack_hex_byte(buf, *tmp);
			tmp++;
			count--;
		}

		*buf = 0;
	}

	return err;
}

/*
 * Copy the binary array pointed to by buf into mem.  Fix $, #, and
 * 0x7d escaped with 0x7d.  Return a pointer to the character after
 * the last byte written.
 */
static int kgdb_ebin2mem(char *buf, char *mem, int count)
{
	int err = 0;
	char c;

	while (count-- > 0) {
		c = *buf++;
		if (c == 0x7d)
			c = *buf++ ^ 0x20;

		err = probe_kernel_write(mem, &c, 1);
		if (err)
			break;

		mem++;
	}

	return err;
}

/*
 * Convert the hex array pointed to by buf into binary to be placed in mem.
 * Return a pointer to the character AFTER the last byte written.
 * May return an error.
 */
int kgdb_hex2mem(char *buf, char *mem, int count)
{
	char *tmp_raw;
	char *tmp_hex;

	/*
	 * We use the upper half of buf as an intermediate buffer for the
	 * raw memory that is converted from hex.
	 */
	tmp_raw = buf + count * 2;

	tmp_hex = tmp_raw - 1;
	while (tmp_hex >= buf) {
		tmp_raw--;
		*tmp_raw = hex(*tmp_hex--);
		*tmp_raw |= hex(*tmp_hex--) << 4;
	}

	return probe_kernel_write(mem, tmp_raw, count);
}

/*
 * While we find nice hex chars, build a long_val.
 * Return number of chars processed.
 */
int kgdb_hex2long(char **ptr, long *long_val)
{
	int hex_val;
	int num = 0;

	*long_val = 0;

	while (**ptr) {
		hex_val = hex(**ptr);
		if (hex_val < 0)
			break;

		*long_val = (*long_val << 4) | hex_val;
		num++;
		(*ptr)++;
	}

	return num;
}

/* Write memory due to an 'M' or 'X' packet. */
static int write_mem_msg(int binary)
{
	char *ptr = &remcom_in_buffer[1];
	unsigned long addr;
	unsigned long length;
	int err;

	if (kgdb_hex2long(&ptr, &addr) > 0 && *(ptr++) == ',' &&
	    kgdb_hex2long(&ptr, &length) > 0 && *(ptr++) == ':') {
		if (binary)
			err = kgdb_ebin2mem(ptr, (char *)addr, length);
		else
			err = kgdb_hex2mem(ptr, (char *)addr, length);
		if (err)
			return err;
		if (CACHE_FLUSH_IS_SAFE)
			flush_icache_range(addr, addr + length + 1);
		return 0;
	}

	return -EINVAL;
}

static void error_packet(char *pkt, int error)
{
	error = -error;
	pkt[0] = 'E';
	pkt[1] = hexchars[(error / 10)];
	pkt[2] = hexchars[(error % 10)];
	pkt[3] = '\0';
}

/*
 * Thread ID accessors. We represent a flat TID space to GDB, where
 * the per CPU idle threads (which under Linux all have PID 0) are
 * remapped to negative TIDs.
 */

#define BUF_THREAD_ID_SIZE	16

static char *pack_threadid(char *pkt, unsigned char *id)
{
	char *limit;

	limit = pkt + BUF_THREAD_ID_SIZE;
	while (pkt < limit)
		pkt = pack_hex_byte(pkt, *id++);

	return pkt;
}

static void int_to_threadref(unsigned char *id, int value)
{
	unsigned char *scan;
	int i = 4;

	scan = (unsigned char *)id;
	while (i--)
		*scan++ = 0;
	*scan++ = (value >> 24) & 0xff;
	*scan++ = (value >> 16) & 0xff;
	*scan++ = (value >> 8) & 0xff;
	*scan++ = (value & 0xff);
}

static struct task_struct *getthread(struct pt_regs *regs, int tid)
{
	return task_lookup(tid);
}

/*
 * CPU debug state control:
 */

#ifdef CONFIG_SMP
static void kgdb_wait(struct pt_regs *regs)
{
	unsigned long flags;
	int cpu;

	local_irq_save(flags);
	cpu = raw_smp_processor_id();
	kgdb_info[cpu].debuggerinfo = regs;
	kgdb_info[cpu].task = current;
	/*
	 * Make sure the above info reaches the primary CPU before
	 * our cpu_in_kgdb[] flag setting does:
	 */
	smp_wmb();
	atomic_set(&cpu_in_kgdb[cpu], 1);

	/*
	 * The primary CPU must be active to enter here, but this is
	 * guard in case the primary CPU had not been selected if
	 * this was an entry via nmi.
	 */
	while (atomic_read(&kgdb_active) == -1)
		cpu_relax();

	/* Wait till primary CPU goes completely into the debugger. */
	while (!atomic_read(&cpu_in_kgdb[atomic_read(&kgdb_active)]))
		cpu_relax();

	/* Wait till primary CPU is done with debugging */
	while (atomic_read(&passive_cpu_wait[cpu]))
		cpu_relax();

	kgdb_info[cpu].debuggerinfo = NULL;
	kgdb_info[cpu].task = NULL;

	/* fix up hardware debug registers on local cpu */
	if (arch_kgdb_ops.correct_hw_break)
		arch_kgdb_ops.correct_hw_break();

	/* Signal the primary CPU that we are done: */
	atomic_set(&cpu_in_kgdb[cpu], 0);
	local_irq_restore(flags);
}
#endif

/*
 * Some architectures need cache flushes when we set/clear a
 * breakpoint:
 */
static void kgdb_flush_swbreak_addr(unsigned long addr)
{
	if (!CACHE_FLUSH_IS_SAFE)
		return;

	if (current->aspace) {
		flush_cache_range(current->aspace,
				  addr, addr + BREAK_INSTR_SIZE);
	} else {
		flush_icache_range(addr, addr + BREAK_INSTR_SIZE);
	}
}

/*
 * SW breakpoint management:
 */
static int kgdb_activate_sw_breakpoints(void)
{
	unsigned long addr;
	int error = 0;
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != BP_SET)
			continue;

		addr = kgdb_break[i].bpt_addr;
		error = kgdb_arch_set_breakpoint(addr,
				kgdb_break[i].saved_instr);
		if (error)
			return error;

		kgdb_flush_swbreak_addr(addr);
		kgdb_break[i].state = BP_ACTIVE;
	}
	return 0;
}

static int kgdb_set_sw_break(unsigned long addr)
{
	int err = kgdb_validate_break_address(addr);
	int breakno = -1;
	int i;

	if (err)
		return err;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == BP_SET) &&
					(kgdb_break[i].bpt_addr == addr))
			return -EEXIST;
	}
	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state == BP_REMOVED &&
					kgdb_break[i].bpt_addr == addr) {
			breakno = i;
			break;
		}
	}

	if (breakno == -1) {
		for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
			if (kgdb_break[i].state == BP_UNDEFINED) {
				breakno = i;
				break;
			}
		}
	}

	if (breakno == -1)
		return -E2BIG;

	kgdb_break[breakno].state = BP_SET;
	kgdb_break[breakno].type = BP_BREAKPOINT;
	kgdb_break[breakno].bpt_addr = addr;

	return 0;
}

static int kgdb_deactivate_sw_breakpoints(void)
{
	unsigned long addr;
	int error = 0;
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != BP_ACTIVE)
			continue;
		addr = kgdb_break[i].bpt_addr;
		error = kgdb_arch_remove_breakpoint(addr,
					kgdb_break[i].saved_instr);
		if (error)
			return error;

		kgdb_flush_swbreak_addr(addr);
		kgdb_break[i].state = BP_SET;
	}
	return 0;
}

static int kgdb_remove_sw_break(unsigned long addr)
{
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == BP_SET) &&
				(kgdb_break[i].bpt_addr == addr)) {
			kgdb_break[i].state = BP_REMOVED;
			return 0;
		}
	}
	return -ENOENT;
}

int kgdb_isremovedbreak(unsigned long addr)
{
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == BP_REMOVED) &&
					(kgdb_break[i].bpt_addr == addr))
			return 1;
	}
	return 0;
}

int remove_all_break(void)
{
	unsigned long addr;
	int error;
	int i;

	/* Clear memory breakpoints. */
	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != BP_SET)
			continue;
		addr = kgdb_break[i].bpt_addr;
		error = kgdb_arch_remove_breakpoint(addr,
				kgdb_break[i].saved_instr);
		if (error)
			return error;
		kgdb_break[i].state = BP_REMOVED;
	}

	/* Clear hardware breakpoints. */
	if (arch_kgdb_ops.remove_all_hw_break)
		arch_kgdb_ops.remove_all_hw_break();

	return 0;
}

/*
 * Remap normal tasks to their real PID, idle tasks to -1 ... -NR_CPUs:
 */
static inline int shadow_pid(int realpid)
{
	if (realpid)
		return realpid;

	return -1-raw_smp_processor_id();
}

static char gdbmsgbuf[BUFMAX + 1];

static void kgdb_msg_write(const char *s, int len)
{
	char *bufptr;
	int wcount;
	int i;

	/* 'O'utput */
	gdbmsgbuf[0] = 'O';

	/* Fill and send buffers... */
	while (len > 0) {
		bufptr = gdbmsgbuf + 1;

		/* Calculate how many this time */
		if ((len << 1) > (BUFMAX - 2))
			wcount = (BUFMAX - 2) >> 1;
		else
			wcount = len;

		/* Pack in hex chars */
		for (i = 0; i < wcount; i++)
			bufptr = pack_hex_byte(bufptr, s[i]);
		*bufptr = '\0';

		/* Move up */
		s += wcount;
		len -= wcount;

		/* Write packet */
		put_packet(gdbmsgbuf);
	}
}

/*
 * Return true if there is a valid kgdb I/O module.  Also if no
 * debugger is attached a message can be printed to the console about
 * waiting for the debugger to attach.
 *
 * The print_wait argument is only to be true when called from inside
 * the core kgdb_handle_exception, because it will wait for the
 * debugger to attach.
 */
static int kgdb_io_ready(int print_wait)
{
	if (!kgdb_io_ops)
		return 0;
	if (kgdb_connected)
		return 1;
	if (atomic_read(&kgdb_setting_breakpoint))
		return 1;
	if (print_wait)
		printk(KERN_CRIT "KGDB: Waiting for remote debugger\n");
	return 1;
}

/*
 * All the functions that start with gdb_cmd are the various
 * operations to implement the handlers for the gdbserial protocol
 * where KGDB is communicating with an external debugger
 */

/* Handle the '?' status packets */
static void gdb_cmd_status(struct kgdb_state *ks)
{
	/*
	 * We know that this packet is only sent
	 * during initial connect.  So to be safe,
	 * we clear out our breakpoints now in case
	 * GDB is reconnecting.
	 */
	remove_all_break();

	remcom_out_buffer[0] = 'S';
	pack_hex_byte(&remcom_out_buffer[1], ks->signo);
}

/* Handle the 'g' get registers request */
static void gdb_cmd_getregs(struct kgdb_state *ks)
{
	struct task_struct *thread;
	void *local_debuggerinfo;
	int i;

	thread = kgdb_usethread;
	if (!thread) {
		thread = kgdb_info[ks->cpu].task;
		local_debuggerinfo = kgdb_info[ks->cpu].debuggerinfo;
	} else {
		local_debuggerinfo = NULL;
		for (i = 0; i < NR_CPUS; i++) {
			/*
			 * Try to find the task on some other
			 * or possibly this node if we do not
			 * find the matching task then we try
			 * to approximate the results.
			 */
			if (thread == kgdb_info[i].task)
				local_debuggerinfo = kgdb_info[i].debuggerinfo;
		}
	}

	/*
	 * All threads that don't have debuggerinfo should be
	 * in __schedule() sleeping, since all other CPUs
	 * are in kgdb_wait, and thus have debuggerinfo.
	 */
	if (local_debuggerinfo) {
		pt_regs_to_gdb_regs(gdb_regs, local_debuggerinfo);
	} else {
		/*
		 * Pull stuff saved during switch_to; nothing
		 * else is accessible (or even particularly
		 * relevant).
		 *
		 * This should be enough for a stack trace.
		 */
		sleeping_thread_to_gdb_regs(gdb_regs, thread);
	}
	kgdb_mem2hex((char *)gdb_regs, remcom_out_buffer, NUMREGBYTES);
}

/* Handle the 'G' set registers request */
static void gdb_cmd_setregs(struct kgdb_state *ks)
{
	kgdb_hex2mem(&remcom_in_buffer[1], (char *)gdb_regs, NUMREGBYTES);

	if (kgdb_usethread && kgdb_usethread != current) {
		error_packet(remcom_out_buffer, -EINVAL);
	} else {
		gdb_regs_to_pt_regs(gdb_regs, ks->lwk_regs);
		strcpy(remcom_out_buffer, "OK");
	}
}

/* Handle the 'm' memory read bytes */
static void gdb_cmd_memread(struct kgdb_state *ks)
{
	char *ptr = &remcom_in_buffer[1];
	unsigned long length;
	unsigned long addr;
	int err;

	if (kgdb_hex2long(&ptr, &addr) > 0 && *ptr++ == ',' &&
					kgdb_hex2long(&ptr, &length) > 0) {
		err = kgdb_mem2hex((char *)addr, remcom_out_buffer, length);
		if (err)
			error_packet(remcom_out_buffer, err);
	} else {
		error_packet(remcom_out_buffer, -EINVAL);
	}
}

/* Handle the 'M' memory write bytes */
static void gdb_cmd_memwrite(struct kgdb_state *ks)
{
	int err = write_mem_msg(0);

	if (err)
		error_packet(remcom_out_buffer, err);
	else
		strcpy(remcom_out_buffer, "OK");
}

/* Handle the 'X' memory binary write bytes */
static void gdb_cmd_binwrite(struct kgdb_state *ks)
{
	int err = write_mem_msg(1);

	if (err)
		error_packet(remcom_out_buffer, err);
	else
		strcpy(remcom_out_buffer, "OK");
}

/* Handle the 'D' or 'k', detach or kill packets */
static void gdb_cmd_detachkill(struct kgdb_state *ks)
{
	int error;

	/* The detach case */
	if (remcom_in_buffer[0] == 'D') {
		error = remove_all_break();
		if (error < 0) {
			error_packet(remcom_out_buffer, error);
		} else {
			strcpy(remcom_out_buffer, "OK");
			kgdb_connected = 0;
		}
		put_packet(remcom_out_buffer);
	} else {
		/*
		 * Assume the kill case, with no exit code checking,
		 * trying to force detach the debugger:
		 */
		remove_all_break();
		kgdb_connected = 0;
	}
}

/* Handle the 'R' reboot packets */
static int gdb_cmd_reboot(struct kgdb_state *ks)
{
	return 0;
}

/* Handle the 'q' query packets */
static void gdb_cmd_query(struct kgdb_state *ks)
{
	struct task_struct *thread;
	unsigned char thref[8];
	char *ptr;
	int i;

	switch (remcom_in_buffer[1]) {
	case 's':
	case 'f':
		if (memcmp(remcom_in_buffer + 2, "ThreadInfo", 10)) {
			error_packet(remcom_out_buffer, -EINVAL);
			break;
		}

		if (remcom_in_buffer[1] == 'f')
			ks->threadid = -CONFIG_NR_CPUS + 1;

		remcom_out_buffer[0] = 'm';
		ptr = remcom_out_buffer + 1;

		for (i = 0; i < 16; ks->threadid++, i++) {
			thread = getthread(ks->lwk_regs, ks->threadid);
			if (thread) {
				int_to_threadref(thref, ks->threadid);
				pack_threadid(ptr, thref);
				ptr += BUF_THREAD_ID_SIZE;
				*(ptr++) = ',';
			}
		}
		*(--ptr) = '\0';
		break;

	case 'C':
		/* Current thread id */
		strcpy(remcom_out_buffer, "QC");
		ks->threadid = shadow_pid(current->id);
		int_to_threadref(thref, ks->threadid);
		pack_threadid(remcom_out_buffer + 2, thref);
		break;
	case 'T':
		if (memcmp(remcom_in_buffer + 1, "ThreadExtraInfo,", 16)) {
			error_packet(remcom_out_buffer, -EINVAL);
			break;
		}
		ks->threadid = 0;
		ptr = remcom_in_buffer + 17;
		kgdb_hex2long(&ptr, &ks->threadid);
		if (!getthread(ks->lwk_regs, ks->threadid)) {
			error_packet(remcom_out_buffer, -EINVAL);
			break;
		}
		if (ks->threadid > 0) {
			kgdb_mem2hex(getthread(ks->lwk_regs,
					ks->threadid)->name,
					remcom_out_buffer, 16);
		} else {
			static char tmpstr[23 + BUF_THREAD_ID_SIZE];

			sprintf(tmpstr, "Shadow task %d for pid 0",
					(int)(-ks->threadid-1));
			kgdb_mem2hex(tmpstr, remcom_out_buffer, strlen(tmpstr));
		}
		break;
	}
}

/* Handle the 'H' task query packets */
static void gdb_cmd_task(struct kgdb_state *ks)
{
	struct task_struct *thread;
	char *ptr;

	switch (remcom_in_buffer[1]) {
	case 'g':
		ptr = &remcom_in_buffer[2];
		kgdb_hex2long(&ptr, &ks->threadid);
		thread = getthread(ks->lwk_regs, ks->threadid);
		if (!thread && ks->threadid > 0) {
			error_packet(remcom_out_buffer, -EINVAL);
			break;
		}
		kgdb_usethread = thread;
		ks->kgdb_usethreadid = ks->threadid;
		strcpy(remcom_out_buffer, "OK");
		break;
	case 'c':
		ptr = &remcom_in_buffer[2];
		kgdb_hex2long(&ptr, &ks->threadid);
		if (!ks->threadid) {
			kgdb_contthread = NULL;
		} else {
			thread = getthread(ks->lwk_regs, ks->threadid);
			if (!thread && ks->threadid > 0) {
				error_packet(remcom_out_buffer, -EINVAL);
				break;
			}
			kgdb_contthread = thread;
		}
		strcpy(remcom_out_buffer, "OK");
		break;
	}
}

/* Handle the 'T' thread query packets */
static void gdb_cmd_thread(struct kgdb_state *ks)
{
	char *ptr = &remcom_in_buffer[1];
	struct task_struct *thread;

	kgdb_hex2long(&ptr, &ks->threadid);
	thread = getthread(ks->lwk_regs, ks->threadid);
	if (thread)
		strcpy(remcom_out_buffer, "OK");
	else
		error_packet(remcom_out_buffer, -EINVAL);
}

/* Handle the 'z' or 'Z' breakpoint remove or set packets */
static void gdb_cmd_break(struct kgdb_state *ks)
{
	/*
	 * Since GDB-5.3, it's been drafted that '0' is a software
	 * breakpoint, '1' is a hardware breakpoint, so let's do that.
	 */
	char *bpt_type = &remcom_in_buffer[1];
	char *ptr = &remcom_in_buffer[2];
	unsigned long addr;
	unsigned long length;
	int error = 0;

	if (arch_kgdb_ops.set_hw_breakpoint && *bpt_type >= '1') {
		/* Unsupported */
		if (*bpt_type > '4')
			return;
	} else {
		if (*bpt_type != '0' && *bpt_type != '1')
			/* Unsupported. */
			return;
	}

	/*
	 * Test if this is a hardware breakpoint, and
	 * if we support it:
	 */
	if (*bpt_type == '1' && !(arch_kgdb_ops.flags & KGDB_HW_BREAKPOINT))
		/* Unsupported. */
		return;

	if (*(ptr++) != ',') {
		error_packet(remcom_out_buffer, -EINVAL);
		return;
	}
	if (!kgdb_hex2long(&ptr, &addr)) {
		error_packet(remcom_out_buffer, -EINVAL);
		return;
	}
	if (*(ptr++) != ',' ||
		!kgdb_hex2long(&ptr, &length)) {
		error_packet(remcom_out_buffer, -EINVAL);
		return;
	}

	if (remcom_in_buffer[0] == 'Z' && *bpt_type == '0')
		error = kgdb_set_sw_break(addr);
	else if (remcom_in_buffer[0] == 'z' && *bpt_type == '0')
		error = kgdb_remove_sw_break(addr);
	else if (remcom_in_buffer[0] == 'Z')
		error = arch_kgdb_ops.set_hw_breakpoint(addr,
			(int)length, *bpt_type);
	else if (remcom_in_buffer[0] == 'z')
		error = arch_kgdb_ops.remove_hw_breakpoint(addr,
			(int) length, *bpt_type);

	if (error == 0)
		strcpy(remcom_out_buffer, "OK");
	else
		error_packet(remcom_out_buffer, error);
}

/* Handle the 'C' signal / exception passing packets */
static int gdb_cmd_exception_pass(struct kgdb_state *ks)
{
	/* C09 == pass exception
	 * C15 == detach kgdb, pass exception
	 */
	if (remcom_in_buffer[1] == '0' && remcom_in_buffer[2] == '9') {

		ks->pass_exception = 1;
		remcom_in_buffer[0] = 'c';

	} else if (remcom_in_buffer[1] == '1' && remcom_in_buffer[2] == '5') {

		ks->pass_exception = 1;
		remcom_in_buffer[0] = 'D';
		remove_all_break();
		kgdb_connected = 0;
		return 1;

	} else {
		error_packet(remcom_out_buffer, -EINVAL);
		return 0;
	}

	/* Indicate fall through */
	return -1;
}

/*
 * This function performs all gdbserial command procesing
 */
static int gdb_serial_stub(struct kgdb_state *ks)
{
	int error = 0;
	int tmp;

	/* Clear the out buffer. */
	memset(remcom_out_buffer, 0, sizeof(remcom_out_buffer));

	if (kgdb_connected) {
		unsigned char thref[8];
		char *ptr;

		/* Reply to host that an exception has occurred */
		ptr = remcom_out_buffer;
		*ptr++ = 'T';
		ptr = pack_hex_byte(ptr, ks->signo);
		ptr += strlen(strcpy(ptr, "thread:"));
		int_to_threadref(thref, shadow_pid(current->id));
		ptr = pack_threadid(ptr, thref);
		*ptr++ = ';';
		put_packet(remcom_out_buffer);
	}

	kgdb_usethread = kgdb_info[ks->cpu].task;
	ks->kgdb_usethreadid = shadow_pid(kgdb_info[ks->cpu].task->id);
	ks->pass_exception = 0;

	while (1) {
		error = 0;

		/* Clear the out buffer. */
		memset(remcom_out_buffer, 0, sizeof(remcom_out_buffer));

		get_packet(remcom_in_buffer);

		switch (remcom_in_buffer[0]) {
		case '?': /* gdbserial status */
			gdb_cmd_status(ks);
			break;
		case 'g': /* return the value of the CPU registers */
			gdb_cmd_getregs(ks);
			break;
		case 'G': /* set the value of the CPU registers - return OK */
			gdb_cmd_setregs(ks);
			break;
		case 'm': /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
			gdb_cmd_memread(ks);
			break;
		case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA..AA */
			gdb_cmd_memwrite(ks);
			break;
		case 'X': /* XAA..AA,LLLL: Write LLLL bytes at address AA..AA */
			gdb_cmd_binwrite(ks);
			break;
			/* kill or detach. KGDB should treat this like a
			 * continue.
			 */
		case 'D': /* Debugger detach */
		case 'k': /* Debugger detach via kill */
			gdb_cmd_detachkill(ks);
			goto default_handle;
		case 'R': /* Reboot */
			if (gdb_cmd_reboot(ks))
				goto default_handle;
			break;
		case 'q': /* query command */
			gdb_cmd_query(ks);
			break;
		case 'H': /* task related */
			gdb_cmd_task(ks);
			break;
		case 'T': /* Query thread status */
			gdb_cmd_thread(ks);
			break;
		case 'z': /* Break point remove */
		case 'Z': /* Break point set */
			gdb_cmd_break(ks);
			break;
		case 'C': /* Exception passing */
			tmp = gdb_cmd_exception_pass(ks);
			if (tmp > 0)
				goto default_handle;
			if (tmp == 0)
				break;
			/* Fall through on tmp < 0 */
		case 'c': /* Continue packet */
		case 's': /* Single step packet */
			if (kgdb_contthread && kgdb_contthread != current) {
				/* Can't switch threads in kgdb */
				error_packet(remcom_out_buffer, -EINVAL);
				break;
			}
			kgdb_activate_sw_breakpoints();
			/* Fall through to default processing */
		default:
default_handle:
			error = kgdb_arch_handle_exception(ks->ex_vector,
						ks->signo,
						ks->err_code,
						remcom_in_buffer,
						remcom_out_buffer,
						ks->lwk_regs);
			/*
			 * Leave cmd processing on error, detach,
			 * kill, continue, or single step.
			 */
			if (error >= 0 || remcom_in_buffer[0] == 'D' ||
			    remcom_in_buffer[0] == 'k') {
				error = 0;
				goto kgdb_exit;
			}

		}

		/* reply to the request */
		put_packet(remcom_out_buffer);
	}

kgdb_exit:
	if (ks->pass_exception)
		error = 1;
	return error;
}

static int kgdb_reenter_check(struct kgdb_state *ks)
{
	unsigned long addr;

	if (atomic_read(&kgdb_active) != raw_smp_processor_id())
		return 0;

	/* Panic on recursive debugger calls: */
	exception_level++;
	addr = kgdb_arch_pc(ks->ex_vector, ks->lwk_regs);
	kgdb_deactivate_sw_breakpoints();

	/*
	 * If the break point removed ok at the place exception
	 * occurred, try to recover and print a warning to the end
	 * user because the user planted a breakpoint in a place that
	 * KGDB needs in order to function.
	 */
	if (kgdb_remove_sw_break(addr) == 0) {
		exception_level = 0;
		kgdb_skipexception(ks->ex_vector, ks->lwk_regs);
		kgdb_activate_sw_breakpoints();
		printk(KERN_CRIT "KGDB: re-enter error: breakpoint removed\n");

		return 1;
	}
	remove_all_break();
	kgdb_skipexception(ks->ex_vector, ks->lwk_regs);

	if (exception_level > 1) {
		printk("Recursive entry to debugger!");
		while(1) {}
	}

	printk(KERN_CRIT "KGDB: re-enter exception: ALL breakpoints killed\n");
	printk("Recursive entry to debugger");
	while(1) {}

	return 1;
}

/*
 * kgdb_handle_exception() - main entry point from a kernel exception
 *
 * Locking hierarchy:
 *	interface locks, if any (begin_session)
 *	kgdb lock (kgdb_active)
 */
int
kgdb_handle_exception(int evector, int signo, int ecode, struct pt_regs *regs)
{
	struct kgdb_state kgdb_var;
	struct kgdb_state *ks = &kgdb_var;
	unsigned long flags;
	int error = 0;
	int i, cpu;

	ks->cpu			= raw_smp_processor_id();
	ks->ex_vector		= evector;
	ks->signo		= signo;
	ks->ex_vector		= evector;
	ks->err_code		= ecode;
	ks->kgdb_usethreadid	= 0;
	ks->lwk_regs		= regs;

	if (kgdb_reenter_check(ks)) {
		printk("kgdb_reenter_check failed!\n");
		return 0; /* Ouch, double exception ! */
	}
acquirelock:
	/*
	 * Interrupts will be restored by the 'trap return' code, except when
	 * single stepping.
	 */
	local_irq_save(flags);

	cpu = raw_smp_processor_id();

	/*
	 * Acquire the kgdb_active lock:
	 */
	while (atomic_cmpxchg(&kgdb_active, -1, cpu) != -1)
		cpu_relax();

	/*
	 * Do not start the debugger connection on this CPU if the last
	 * instance of the exception handler wanted to come into the
	 * debugger on a different CPU via a single step
	 */
	if (atomic_read(&kgdb_cpu_doing_single_step) != -1 &&
	    atomic_read(&kgdb_cpu_doing_single_step) != cpu) {

		atomic_set(&kgdb_active, -1);
		local_irq_restore(flags);

		goto acquirelock;
	}

	if (!kgdb_io_ready(1)) {
		printk("KGDB resuming because I/O not ready.\n");
		error = 1;
		goto kgdb_restore; /* No I/O connection, so resume the system */
	}

	/*
	 * Don't enter if we have hit a removed breakpoint.
	 */
	if (kgdb_skipexception(ks->ex_vector, ks->lwk_regs))
		goto kgdb_restore;

	/* Call the I/O driver's pre_exception routine */
	if (kgdb_io_ops->pre_exception)
		kgdb_io_ops->pre_exception();

	kgdb_info[ks->cpu].debuggerinfo = ks->lwk_regs;
	kgdb_info[ks->cpu].task = current;

	kgdb_disable_hw_debug(ks->lwk_regs);

	/*
	 * Get the passive CPU lock which will hold all the non-primary
	 * CPU in a spin state while the debugger is active
	 */
	if (!kgdb_single_step || !kgdb_contthread) {
		for (i = 0; i < NR_CPUS; i++)
			atomic_set(&passive_cpu_wait[i], 1);
	}

#if defined(CONFIG_SMP) && 0
	/* Signal the other CPUs to enter kgdb_wait() */
	if (!kgdb_single_step || !kgdb_contthread)
		kgdb_roundup_cpus(flags);
#endif

	/*
	 * spin_lock code is good enough as a barrier so we don't
	 * need one here:
	 */
	atomic_set(&cpu_in_kgdb[ks->cpu], 1);

#if 0
	/*
	 * Wait for the other CPUs to be notified and be waiting for us:
	 */
	for_each_online_cpu(i) {
		while (!atomic_read(&cpu_in_kgdb[i]))
			cpu_relax();
	}
#endif

	/*
	 * At this point the primary processor is completely
	 * in the debugger and all secondary CPUs are quiescent
	 */
	kgdb_post_primary_code(ks->lwk_regs, ks->ex_vector, ks->err_code);
	kgdb_deactivate_sw_breakpoints();
	kgdb_single_step = 0;
	kgdb_contthread = NULL;
	exception_level = 0;

	/* Talk to debugger with gdbserial protocol */
	error = gdb_serial_stub(ks);

	/* Call the I/O driver's post_exception routine */
	if (kgdb_io_ops->post_exception)
		kgdb_io_ops->post_exception();

	kgdb_info[ks->cpu].debuggerinfo = NULL;
	kgdb_info[ks->cpu].task = NULL;
	atomic_set(&cpu_in_kgdb[ks->cpu], 0);

#if 0
	if (!kgdb_single_step || !kgdb_contthread) {
		for (i = NR_CPUS-1; i >= 0; i--)
			atomic_set(&passive_cpu_wait[i], 0);
		/*
		 * Wait till all the CPUs have quit
		 * from the debugger.
		 */
		for_each_online_cpu(i) {
			while (atomic_read(&cpu_in_kgdb[i]))
				cpu_relax();
		}
	}
#endif

kgdb_restore:
	/* Free kgdb_active */
	atomic_set(&kgdb_active, -1);
	local_irq_restore(flags);

	return error;
}

int kgdb_nmicallback(int cpu, void *regs)
{
#ifdef CONFIG_SMP
	if (!atomic_read(&cpu_in_kgdb[cpu]) &&
			atomic_read(&kgdb_active) != cpu) {
		kgdb_wait((struct pt_regs *)regs);
		return 0;
	}
#endif
	return 1;
}

void kgdb_console_write(struct console *co, const char *s)
{
	unsigned long flags;

	/* If we're debugging, or KGDB has not connected, don't try
	 * and print. */
	if (!kgdb_connected || atomic_read(&kgdb_active) != -1)
		return;

	local_irq_save(flags);
	kgdb_msg_write(s, strlen(s));
	local_irq_restore(flags);
}

static struct console kgdbcons = {
	.name		= "kgdb",
	.write		= kgdb_console_write,

	/*.flags		= CON_PRINTBUFFER | CON_ENABLED, */
	/*.index		= -1, */
};

static void kgdb_register_callbacks(void)
{
	if (!kgdb_io_module_registered) {
		kgdb_io_module_registered = 1;
		kgdb_arch_init();
		if (kgdb_use_con && !kgdb_con_registered) {
			console_register(&kgdbcons);
			kgdb_con_registered = 1;
		}
	}
}

void kgdb_initial_breakpoint(void)
{
	if (!kgdb_break_asap)
		return;

	kgdb_break_asap = 0;
	printk(KERN_CRIT "kgdb: Waiting for connection from remote gdb...\n");
	kgdb_breakpoint();
}

/**
 *	kkgdb_register_io_module - register KGDB IO module
 *	@new_kgdb_io_ops: the io ops vector
 *
 *	Register it with the KGDB core.
 */
int kgdb_register_io_module(struct kgdb_io *new_kgdb_io_ops)
{
	int err;

	spin_lock(&kgdb_registration_lock);

	if (kgdb_io_ops) {
		spin_unlock(&kgdb_registration_lock);

		printk(KERN_ERR "kgdb: Another I/O driver is already "
				"registered with KGDB.\n");
		return -EBUSY;
	}

	if (new_kgdb_io_ops->init) {
		err = new_kgdb_io_ops->init();
		if (err) {
			spin_unlock(&kgdb_registration_lock);
			return err;
		}
	}

	kgdb_io_ops = new_kgdb_io_ops;

	spin_unlock(&kgdb_registration_lock);

	printk(KERN_INFO "kgdb: Registered I/O driver %s.\n",
	       new_kgdb_io_ops->name);

	/* Arm KGDB now. */
	kgdb_register_callbacks();

	return 0;
}
EXPORT_SYMBOL_GPL(kgdb_register_io_module);

/**
 * kgdb_breakpoint - generate breakpoint exception
 *
 * This function will generate a breakpoint exception.  It is used at the
 * beginning of a program to sync up with a debugger and can be used
 * otherwise as a quick means to stop program execution and "break" into
 * the debugger.
 */
void kgdb_breakpoint(void)
{
	atomic_set(&kgdb_setting_breakpoint, 1);
	wmb(); /* Sync point before breakpoint */
	arch_kgdb_breakpoint();
	wmb(); /* Sync point after breakpoint */
	atomic_set(&kgdb_setting_breakpoint, 0);
}
EXPORT_SYMBOL_GPL(kgdb_breakpoint);

static int __init opt_kgdb_wait(char *str)
{
	kgdb_break_asap = 1;

	if (kgdb_io_module_registered)
		kgdb_initial_breakpoint();

	return 0;
}

param_func(kgdbwait, (param_set_fn)opt_kgdb_wait);

/*
 * This provides the callbacks and functions that GDB needs to share between
 * the core, I/O and arch-specific portions.
 *
 * Author: Amit Kale <amitkale@linsyssoft.com> and
 *         Tom Rini <trini@kernel.crashing.org>
 *         modified for Kitten by Patrick Bridges <bridges@cs.unm.edu>
 *
 * 2001-2004 (c) Amit S. Kale and 2003-2005 (c) MontaVista Software, Inc.
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */
#ifndef _GDB_H_
#define _GDB_H_

/* #include <lwk/serial.h> */
#include <lwk/linkage.h>
#include <lwk/init.h>
#include <lwk/ptrace.h>
#include <lwk/task.h>

#include <arch/atomic.h>
#include <arch/gdb.h>

struct pt_regs;
struct tasklet_struct;
struct task_struct;
struct uart_port;

enum gdb_bptype {
	GDB_BP_BREAKPOINT = 0,
	GDB_BP_HARDWARE_BREAKPOINT,
	GDB_BP_WRITE_WATCHPOINT,
	GDB_BP_READ_WATCHPOINT,
	GDB_BP_ACCESS_WATCHPOINT
};

enum gdb_bpstate {
	GDB_BP_UNDEFINED = 0,
	GDB_BP_REMOVED,
	GDB_BP_SET,
	GDB_BP_ACTIVE
};

struct gdb_bkpt {
	unsigned long		bpt_addr;
	unsigned char		saved_instr[BREAK_INSTR_SIZE];
	enum gdb_bptype	type;
	enum gdb_bpstate	state;
};

/*
 *	gdb_skipexception - Bail of of GDB when we've been triggered.
 *	@exception: Exception vector number
 *	@regs: Current &struct pt_regs.
 *
 *	On some architectures we need to skip a breakpoint exception when
 *	it occurs after a breakpoint has been removed.
 */
extern int gdb_skipexception(struct gdb_bkpt *gdb_breaks, int aspace_id, int exception, struct pt_regs *regs);

/*
 *	gdb_post_primary_code - Save error vector/code numbers.
 *	@regs: Original pt_regs.
 *	@e_vector: Original error vector.
 *	@err_code: Original error code.
 *
 *	This is needed on architectures which support SMP and GDB.
 *	This function is called after all the secondary cpus have been put
 *	to a know spin state and the primary CPU has control over GDB.
 */
extern void gdb_post_primary_code(struct pt_regs *regs, int e_vector,
				  int err_code);

/*
 *	gdb_disable_hw_debug - Disable hardware debugging while we in gdb.
 *	@regs: Current &struct pt_regs.
 *
 *	This function will be called if the particular architecture must
 *	disable hardware debugging while it is processing gdb packets or
 *	handling exception.
 */
extern void gdb_disable_hw_debug(struct pt_regs *regs);

#ifndef GDB_MAX_BREAKPOINTS
# define GDB_MAX_BREAKPOINTS	1000
#endif

#define GDB_HW_BREAKPOINT	1

/*
 * Functions each GDB-supporting architecture must provide:
 */

/*
 *	gdb_arch_init - Perform any architecture specific initalization.
 *
 *	This function will handle the initalization of any architecture
 *	specific callbacks.
 */
extern int gdb_arch_init(void);

/*
 *	gdb_arch_exit - Perform any architecture specific uninitalization.
 *
 *	This function will handle the uninitalization of any architecture
 *	specific callbacks, for dynamic registration and unregistration.
 */
extern void gdb_arch_exit(void);

/*
 *	pt_regs_to_gdb_regs - Convert ptrace regs to GDB regs
 *	@gdb_regs: A pointer to hold the registers in the order GDB wants.
 *	@regs: The &struct pt_regs of the current process.
 *
 *	Convert the pt_regs in @regs into the format for registers that
 *	GDB expects, stored in @gdb_regs.
 */
extern void pt_regs_to_gdb_regs(unsigned long *gdb_regs, struct pt_regs *regs);

/*
 *	sleeping_thread_to_gdb_regs - Convert ptrace regs to GDB regs
 *	@gdb_regs: A pointer to hold the registers in the order GDB wants.
 *	@p: The &struct task_struct of the desired process.
 *
 *	Convert the register values of the sleeping process in @p to
 *	the format that GDB expects.
 *	This function is called when gdb does not have access to the
 *	&struct pt_regs and therefore it should fill the gdb registers
 *	@gdb_regs with what has	been saved in &struct thread_struct
 *	thread field during switch_to.
 */
/*extern void
sleeping_thread_to_gdb_regs(unsigned long *gdb_regs, struct task_struct *p);
*/
/*
 *	gdb_regs_to_pt_regs - Convert GDB regs to ptrace regs.
 *	@gdb_regs: A pointer to hold the registers we've received from GDB.
 *	@regs: A pointer to a &struct pt_regs to hold these values in.
 *
 *	Convert the GDB regs in @gdb_regs into the pt_regs, and store them
 *	in @regs.
 */
extern void gdb_regs_to_pt_regs(unsigned long *gdb_regs, struct pt_regs *regs);

/*
 *	gdb_arch_handle_exception - Handle architecture specific GDB packets.
 *	@vector: The error vector of the exception that happened.
 *	@signo: The signal number of the exception that happened.
 *	@err_code: The error code of the exception that happened.
 *	@remcom_in_buffer: The buffer of the packet we have read.
 *	@remcom_out_buffer: The buffer of %BUFMAX bytes to write a packet into.
 *	@regs: The &struct pt_regs of the current process.
 *
 *	This function MUST handle the 'c' and 's' command packets,
 *	as well packets to set / remove a hardware breakpoint, if used.
 *	If there are additional packets which the hardware needs to handle,
 *	they are handled here.  The code should return -1 if it wants to
 *	process more packets, and a %0 or %1 if it wants to exit from the
 *	gdb callback.
 */
extern int
gdb_arch_handle_exception(int vector, int signo, int err_code,
			   struct pt_regs *regs,
			   char *remcom_in_buffer,
			   char *remcom_out_buffer,
			   atomic_t *gdb_cpu_single_step);

/*
 *	gdb_roundup_cpus - Get other CPUs into a holding pattern
 *	@flags: Current IRQ state
 *
 *	On SMP systems, we need to get the attention of the other CPUs
 *	and get them be in a known state.  This should do what is needed
 *	to get the other CPUs to call gdb_wait(). Note that on some arches,
 *	the NMI approach is not used for rounding up all the CPUs. For example,
 *	in case of MIPS, smp_call_function() is used to roundup CPUs. In
 *	this case, we have to make sure that interrupts are enabled before
 *	calling smp_call_function(). The argument to this function is
 *	the flags that will be used when restoring the interrupts. There is
 *	local_irq_save() call before gdb_roundup_cpus().
 *
 *	On non-SMP systems, this is not called.
 */
//extern void gdb_roundup_cpus(unsigned long flags);

/* Optional functions. */
extern int gdb_validate_break_address(unsigned long addr);
extern int gdb_arch_set_breakpoint(unsigned long addr, char *saved_instr);
extern int gdb_arch_remove_breakpoint(unsigned long addr, char *bundle);

/*
 * struct gdb_arch - Describe architecture specific values.
 * @gdb_bpt_instr: The instruction to trigger a breakpoint.
 * @flags: Flags for the breakpoint, currently just %GDB_HW_BREAKPOINT.
 * @set_breakpoint: Allow an architecture to specify how to set a software
 * breakpoint.
 * @remove_breakpoint: Allow an architecture to specify how to remove a
 * software breakpoint.
 * @set_hw_breakpoint: Allow an architecture to specify how to set a hardware
 * breakpoint.
 * @remove_hw_breakpoint: Allow an architecture to specify how to remove a
 * hardware breakpoint.
 * @remove_all_hw_break: Allow an architecture to specify how to remove all
 * hardware breakpoints.
 * @correct_hw_break: Allow an architecture to specify how to correct the
 * hardware debug registers.
 */
struct gdb_arch {
	unsigned char		gdb_bpt_instr[BREAK_INSTR_SIZE];
	unsigned long		flags;

	int	(*set_breakpoint)(unsigned long, char *);
	int	(*remove_breakpoint)(unsigned long, char *);
	int	(*set_hw_breakpoint)(unsigned long, int, enum gdb_bptype);
	int	(*remove_hw_breakpoint)(unsigned long, int, enum gdb_bptype);
	void	(*remove_all_hw_break)(void);
	void	(*correct_hw_break)(void);
};

/*
 * struct gdb_io - Describe the interface for an I/O driver to talk with GDB.
 * @name: Name of the I/O driver.
 * @read_char: Pointer to a function that will return one char.
 * @write_char: Pointer to a function that will write one char.
 * @flush: Pointer to a function that will flush any pending writes.
 * @init: Pointer to a function that will initialize the device.
 * @pre_exception: Pointer to a function that will do any prep work for
 * the I/O driver.
 * @post_exception: Pointer to a function that will do any cleanup work
 * for the I/O driver.
 */
/*struct gdb_io {
	const char		*name;
	int			(*read_char) (void);
	void			(*write_char) (u8);
	void			(*flush) (void);
	int			(*init) (void);
	void        (*deinit) (void);
    void			(*pre_exception) (void);
	void			(*post_exception) (void);
};
*/
int gdb_attach_process(id_t pid, char *gdb_cons);
int gdb_attach_thread(id_t pid, id_t tid, char *gdb_cons);

int sys_gdb_attach_process(id_t pid, char __user *gdb_cons);
int sys_gdb_attach_thread(id_t pid, id_t tid, char __user *gdb_cons);

extern struct gdb_arch		arch_gdb_ops;

extern int gdb_hex2long(char **ptr, long *long_val);
extern int gdb_mem2hex(char *mem, char *buf, int count);
extern int gdb_hex2mem(char *buf, char *mem, int count);

extern int gdb_isremovedbreak(struct gdb_bkpt *gdb_breaks, id_t aspace_id, unsigned long addr);

extern int
gdb_handle_exception(int ex_vector, int signo, int err_code,
		      struct pt_regs *regs);
/*extern int gdb_nmicallback(int cpu, void *regs);
*/
#endif /* _GDB_H_ */

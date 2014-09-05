/*
 * GDB stub.
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
 * Original GDB stub: David Grothe <dave@gcom.com>,
 * Tigran Aivazian <tigran@sco.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */
#include <lwk/driver.h>
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
#include <lwk/gdb.h>
#include <lwk/smp.h>
#include <lwk/params.h>
#include <lwk/linux_compat.h>
#include <lwk/kfs.h>
#include <lwk/gdbio.h>
#include <lwk/htable.h>
#include <lwk/list.h>
#include <lwk/driver.h>
#include <lwk/xcall.h>
#include <lwk/kthread.h>

#include <arch-generic/fcntl.h>
#include <arch/byteorder.h>
#include <arch/atomic.h>
#include <arch/cacheflush.h>
#include <arch/system.h>
#include <arch/uaccess.h>
#include <arch/gdb.h>

struct gdb_per_thread_state {//gdb per thread state
	int			ex_vector;
	int			signo;
	int			err_code;
	int			cpu;
	int			pass_exception;
	long			threadid;
	struct pt_regs		*lwk_regs;
};

struct gdb_per_process_state {//gdb per process state, the following states are shared by all threads belonging to the process
	id_t    pid;    //process id
	struct hlist_node   ht_link;

	struct task_struct  *gdb_usethread;
	struct task_struct  *gdb_contthread;

	struct gdb_bkpt	gdb_break[GDB_MAX_BREAKPOINTS]; 
	struct file *gdb_filep;

	/*
	* The CPU# of the active CPU, or -1 if none:
	*/
	atomic_t			gdb_active; //serializing breakpoint hits from multiple threads at the same time
	int                 gdb_connected;
	//int				    gdb_single_step = 0;
	/* to keep track of the CPU which is doing the single stepping*/
	atomic_t			gdb_cpu_doing_single_step;

	/* Our I/O buffers. */
	char		remcom_in_buffer[GDBIO_BUFFER_SIZE];
	char		remcom_out_buffer[GDBIO_BUFFER_SIZE];

	/* Storage for the registers, in GDB format. */
	unsigned long		gdb_regs[(GDB_NUM_REGBYTES + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
    
	//static unsigned int gdb_stop_cpus;
	/*
	* We use NR_CPUs not PERCPU, in case gdb is used to debug early
	* bootup code (which might not have percpu set up yet):
	*/
	//static atomic_t			passive_cpu_wait[NR_CPUS];
	//static atomic_t			cpu_in_gdb[NR_CPUS];
};
//mappings between gdb global state and task being debuged
static struct htable *gdb_htable = NULL;
static spinlock_t htable_lock;
static atomic_t htable_inited = ATOMIC_INIT(0);

static atomic_t pt_regs_task_offset = ATOMIC_INIT(-1);
//TODO always right???
#define TASK_PTR(pt_regs) ((struct task_struct *)((void *) pt_regs - atomic_read(&pt_regs_task_offset)))

/*
 * Finally, some GDB code :-)
 */
/*
 * gdb_htable contains info about the GDB global state.
 */
static uint64_t gdb_hash_pid(const void *pid, size_t order)
{
	return (* ((id_t *) pid)) % (1<<order);    
}

static int gdb_key_compare(const void *key_in_search, const void *key_in_table){
	id_t * id_in_search = (id_t *) key_in_search;
	id_t * id_in_table = (id_t *) key_in_table;  
	if(*id_in_search == *id_in_table){
		return 0;
	}else if(*id_in_search < *id_in_table){
		return -1;    
	}
	return 1;
}

static void gdb_htable_add(struct gdb_per_process_state *ggs){
	spin_lock(&htable_lock); 
	htable_add(gdb_htable, ggs);
	spin_unlock(&htable_lock);
}

static void gdb_htable_del(struct gdb_per_process_state *ggs){
	spin_lock(&htable_lock); 
	htable_del(gdb_htable, ggs);
	spin_unlock(&htable_lock);
}

static struct gdb_per_process_state *gdb_htable_lookup(id_t pid){
	spin_lock(&htable_lock); 
	struct gdb_per_process_state *ggs = htable_lookup(gdb_htable, &pid);
	spin_unlock(&htable_lock);
	return ggs;
}

//GDB IO related code
static struct file *gdbio_init(char *gdb_cons){
	if(mk_gdb_fifo(gdb_cons) < 0){ 
		return NULL;
	}
	return  open_gdb_fifo(gdb_cons);
}

//block reading
static char gdbio_get_char(struct file *gdb_filep){
	char ch;
	gdbserver_read(gdb_filep, &ch, 1);
	return ch;
}

//block writing
static void gdbio_write_char(struct file *gdb_filep, char ch){
	gdbserver_write(gdb_filep, &ch, 1);
}

  
/*
 * Weak aliases for breakpoint management,
 * can be overriden by architectures when needed:
 */
/*int __weak gdb_validate_break_address(unsigned long addr)
{
    char tmp_variable[BREAK_INSTR_SIZE];

    return probe_kernel_read(tmp_variable, (char *)addr, BREAK_INSTR_SIZE);
}*/
/*
 * Replace the first byte of the instruction with INT3(cc)
 */
int __weak gdb_arch_set_breakpoint(unsigned long addr, char *saved_instr){
	int err = probe_kernel_read(saved_instr, (char *)addr, BREAK_INSTR_SIZE);
	if (err) return err;

	return probe_kernel_write((char *)addr, arch_gdb_ops.gdb_bpt_instr, BREAK_INSTR_SIZE);
}
/*
 * Restore the instruction repalced by gdb_arch_set_breakpoint func
 * */
int __weak gdb_arch_remove_breakpoint(unsigned long addr, char *bundle){
	return probe_kernel_write((char *)addr, (char *)bundle, BREAK_INSTR_SIZE);
}

unsigned long __weak gdb_arch_pc(int exception, struct pt_regs *regs){
	return instruction_pointer(regs);
}

int __weak gdb_arch_init(void){
	return 0;
}

/**
 *	gdb_disable_hw_debug - Disable hardware debugging while we in gdb.
 *	@regs: Current &struct pt_regs.
 *
 *	This function will be called if the particular architecture must
 *	disable hardware debugging while it is processing gdb packets or
 *	handling exception.
 */
void __weak gdb_disable_hw_debug(struct pt_regs *regs){
	//set_debugreg(0UL, 7);
}

/*
 * GDB remote protocol parser:
 */

static const char	hexchars[] = "0123456789abcdef";

static int hex(char ch){
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	if ((ch >= 'A') && (ch <= 'F'))
		return ch - 'A' + 10;
	return -1;
}


/* scan for the sequence $<data>#<checgsum> */
static void gdb_get_packet(struct gdb_per_process_state *ggs)
{
	unsigned char checgsum;
	unsigned char xmitcsum;
	int count;
	char ch;
	do {
        
		while(gdbio_get_char(ggs->gdb_filep) != '$')
			ggs->gdb_connected = 1;
		checgsum = 0;
		xmitcsum = -1;
		count = 0;
		memset(ggs->remcom_in_buffer, 0, sizeof(ggs->remcom_in_buffer));	
        
		/*
		 * now, read until a # or end of buffer is found:
		 */
		while (count < (GDBIO_BUFFER_SIZE - 1)) {
			ch = gdbio_get_char(ggs->gdb_filep);
			if (ch == '#')
				break;
			ggs->remcom_in_buffer[count++] = ch;
			checgsum = checgsum + ch;
		}
		ggs->remcom_in_buffer[count] = 0;

		if (ch == '#') {
			xmitcsum = hex(gdbio_get_char(ggs->gdb_filep)) << 4;
			xmitcsum += hex(gdbio_get_char(ggs->gdb_filep));

			if (checgsum != xmitcsum) {
				//printk("GDB: packet(%s) is invalid\n", buffer);
				gdbio_write_char(ggs->gdb_filep, '-');
			} else {
				//printk("GDB: packet(%s) is valid\n", buffer);
				gdbio_write_char(ggs->gdb_filep, '+');
			}
		}
	} while (checgsum != xmitcsum);
	//printk("GDB Request<2>: %s\n", buffer);
}

/*
 * Send the packet in buffer.
 * Check for gdb connection if asked for.
 */
static void gdb_put_packet(struct gdb_per_process_state *ggs)
{
	//printk("GDB Reply<2>: %s\n", buffer);
	unsigned char checgsum;
	int count;
	char ch;
	/*
	 * $<packet info>#<checgsum>.
	 */
	while (1) {
		checgsum = 0;
		count = 0;

		gdbio_write_char(ggs->gdb_filep, '$');
		while ((ch = ggs->remcom_out_buffer[count])) {
			gdbio_write_char(ggs->gdb_filep, ch);
			checgsum += ch;
			count++;
		}
		gdbio_write_char(ggs->gdb_filep, '#');
		gdbio_write_char(ggs->gdb_filep, hexchars[checgsum >> 4]);
		gdbio_write_char(ggs->gdb_filep, hexchars[checgsum & 0xf]);

		/* Now see what we get in reply. */
		ch = gdbio_get_char(ggs->gdb_filep);
		
		if (ch == 3){
			ch = gdbio_get_char(ggs->gdb_filep);
		}
        
		/* If we get an ACK, we are done. */
		if (ch == '+'){
			//printk("GDB: send reply(%s) successfully\n", buffer);
			return;
		}
        
		/*
		 * If we get the start of another packet, this means
		 * that GDB is attempting to reconnect.  We will NAK
		 * the packet being sent, and stop trying to send this
		 * packet.
		 */
		if (ch == '$') {
			//printk("GDB: send reply(%s) failed\n", buffer);
			gdbio_write_char(ggs->gdb_filep, '-');
			return;
		}

		//printk("GDB: resending reply(%s)\n", buffer);
	}
}

static char *pack_hex_byte(char *pkt, u8 byte){
	*pkt++ = hexchars[byte >> 4];
	*pkt++ = hexchars[byte & 0xf];

	return pkt;
}

static unsigned long userspace_vaddr2kenel_vaddr(id_t aspace_id, unsigned long userspace_vaddr){
	addr_t paddr;

	aspace_virt_to_phys(aspace_id, userspace_vaddr, &paddr);
	return (unsigned long) __va(paddr);
}

/*
 * @param:
 *      mem: kernel virtual address
 *      buf: kernel virtual address
 * Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null). May return an error.
 */
int gdb_mem2hex(char *mem, char *buf, int count)
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
 * @param:
 *      buf: kernel virtual address
 *      mem: kernel virtual address
 * Copy the binary array pointed to by buf into mem.  Fix $, #, and
 * 0x7d escaped with 0x7d.  Return a pointer to the character after
 * the last byte written.
 */
static int gdb_ebin2mem(char *buf, char *mem, int count)
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
int gdb_hex2mem(char *buf, char *mem, int count)
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
int gdb_hex2long(char **ptr, long *long_val)
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
static int write_mem_msg(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs, int binary)
{
	char *ptr = &ggs->remcom_in_buffer[1];
	unsigned long addr;
	unsigned long length;
	int err;

	if (gdb_hex2long(&ptr, &addr) > 0 && *(ptr++) == ',' &&
		gdb_hex2long(&ptr, &length) > 0 && *(ptr++) == ':') {
		/*Address translationg: because gdbserver needs to write debugie process's memory*/
		addr = userspace_vaddr2kenel_vaddr(ggs->pid, addr);
		if(!addr){
			return -EINVAL;
		} 
		if (binary)
			err = gdb_ebin2mem(ptr, (char *)addr, length);
		else
			err = gdb_hex2mem(ptr, (char *)addr, length);
		if (err) return err;
		if (CACHE_FLUSH_IS_SAFE)
			flush_icache_range(addr, addr + length + 1);
		return 0;
	}

	return -EINVAL;
}

static void error_packet(char *pkt, int error){
	error = -error;
	pkt[0] = 'E';
	pkt[1] = hexchars[(error / 10)];
	pkt[2] = hexchars[(error % 10)];
	pkt[3] = '\0';
}


#define BUF_THREAD_ID_SIZE	16

static char *pack_threadid(char *pkt, unsigned char *id){
	char *limit;

	limit = pkt + BUF_THREAD_ID_SIZE;
	while (pkt < limit)
		pkt = pack_hex_byte(pkt, *id++);

	return pkt;
}

static void int_to_threadref(unsigned char *id, int value){
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

struct task_struct *get_task(id_t pid, id_t tid){
	struct task_struct *tsk, *tskret = NULL;
	struct aspace *aspace = aspace_acquire(pid);
	if(aspace){
		list_for_each_entry(tsk, &aspace->task_list, aspace_link) {
			if (tsk->id == tid)
				tskret = tsk;
		}
	}
	aspace_release(aspace);
	return tskret; 
}

#define GDB_STOP_TASK 0
#define GDB_RESUME_TASK 1
/*
 * Even if the task is in TASK_(UN)INTERRUPTIBLE state, it still works! 
 * Becuase these tasks are wakeup via sched_wakeup_task() api, which only wakeup tasks in TASK_(UN)INTERRUPTIBLE state.
 **/
static void gdb_stop_or_resume_single_task(struct task_struct *task, int req_type){
	unsigned long irqstate;
	spin_lock_irqsave(&task->aspace->lock, irqstate); 
	if(req_type == GDB_STOP_TASK){//stop a running task
		if(task->state != TASK_STOPPED){
			task->ptrace = (task->state << 1) | 1; 	//bit 0 of ptrace flag indicates the task is being traced, 
								//bit 1~3 of ptrace flag indicates task state before it is stopped	
			set_task_state(task, TASK_STOPPED);
			//printk("GDB: Task(pid=%d, tid=%d) stopped with ptrace=%d\n)", task->aspace->id, task->id, task->ptrace);
		}
	}else{//resume a stopped task
		if(task->state == TASK_STOPPED){//TODO for tasks in TASK_(UN)INTERRUPTIBLE, shouldn't simply set it's state to TASK_RUNNING
			//struct pt_regs *regs = task_pt_regs(&task->arch);
			//printk("GDB: Resume from %p TF=%lu\n", (void *) regs->rip, regs->eflags & TF_MASK);
			set_task_state(task, task->ptrace >> 1);
			task->ptrace = 0;
			//printk("GDB: Task(pid=%d, tid=%d) resume with task->state=%d", task->aspace->id, task->id, task->state); 
		}   
	}
	xcall_reschedule(task->cpu_id);
	spin_unlock_irqrestore(&task->aspace->lock, irqstate);
}

static void gdb_stop_or_resume_all(struct task_struct *task, int req_type){
        if(task == NULL){
                return;
        }    

        struct task_struct *tsk;
        struct aspace *aspace = aspace_acquire(task->aspace->id);
        list_for_each_entry(tsk, &aspace->task_list, aspace_link) {
                gdb_stop_or_resume_single_task(tsk, req_type);
        }    
        aspace_release(aspace);
}

static void gdb_stop_or_resume_all_except(struct task_struct *task, id_t task_id, int req_type){
        if(task == NULL){
                return;
        }    

        struct task_struct *tsk;
        struct aspace *aspace = aspace_acquire(task->aspace->id);
        list_for_each_entry(tsk, &aspace->task_list, aspace_link) {
                if(tsk->id == task_id){
                        continue;
                }    
                gdb_stop_or_resume_single_task(tsk, req_type);
        }    
        aspace_release(aspace);
}

/*
 * Some architectures need cache flushes when we set/clear a
 * breakpoint:
 */
static void gdb_flush_swbreak_addr(unsigned long kernel_vaddr){
	if (!CACHE_FLUSH_IS_SAFE)
		return;

	if (current->aspace) {
		flush_cache_range(current->aspace, kernel_vaddr, kernel_vaddr + BREAK_INSTR_SIZE);
	} else {
		flush_icache_range(kernel_vaddr, kernel_vaddr + BREAK_INSTR_SIZE);
	}
}

/*
 * SW breakpoint management:
 */
static int gdb_activate_sw_breakpoints(struct gdb_bkpt *gdb_break){
	unsigned long addr;
	int error = 0;
	int i;

	for (i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
		if (gdb_break[i].state != GDB_BP_SET)
			continue;

		addr = gdb_break[i].bpt_addr;
		error = gdb_arch_set_breakpoint(addr,
				gdb_break[i].saved_instr);
		if (error) return error;

		gdb_flush_swbreak_addr(addr);
		gdb_break[i].state = GDB_BP_ACTIVE;
		//printk("GDB: activate breakpoint at addr: %p\n", (void *) addr);
	}
	return 0;
}

static int gdb_set_sw_break(struct gdb_bkpt	*gdb_break, id_t aspace_id, unsigned long u_vaddr)
{ 
	//Address translation: because gdbserver needs to read/write debugie process's address space	
	int breakno = -1;
	int i;

	unsigned long kernel_vaddr = userspace_vaddr2kenel_vaddr(aspace_id, u_vaddr);
	if(!kernel_vaddr){
		return -EINVAL;    
	} 
	//printk("GDB: set breakpoint at addr(%p %p)\n", (void *) u_vaddr, (void *) kernel_vaddr);

	for (i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
		if ((gdb_break[i].state == GDB_BP_SET) &&
			(gdb_break[i].bpt_addr == kernel_vaddr))
			return -EEXIST;
	}
	for (i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
		if (gdb_break[i].state == GDB_BP_REMOVED &&
			gdb_break[i].bpt_addr == kernel_vaddr) {
			breakno = i;
			break;
		}
	}

	if (breakno == -1) {
		for (i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
			if (gdb_break[i].state == GDB_BP_UNDEFINED) {
				breakno = i;
				break;
			}
		}
	}

	if (breakno == -1)
		return -E2BIG;

	gdb_break[breakno].state = GDB_BP_SET;
	gdb_break[breakno].type = GDB_BP_BREAKPOINT;
	gdb_break[breakno].bpt_addr = kernel_vaddr;

	return 0;
}

static int gdb_deactivate_sw_breakpoints(struct gdb_bkpt *gdb_break){
	unsigned long addr;
	int error = 0;
	int i;

	for (i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
		if (gdb_break[i].state != GDB_BP_ACTIVE)
			continue;
		addr = gdb_break[i].bpt_addr;
		error = gdb_arch_remove_breakpoint(addr,
					gdb_break[i].saved_instr);
		if (error)
			return error;

		gdb_flush_swbreak_addr(addr);
		gdb_break[i].state = GDB_BP_SET;
		//printk("GDB: deactivate breakpoint at addr: %p\n", (void *) addr);
	}
	return 0;
}

static int gdb_remove_sw_break(struct gdb_bkpt *gdb_break, id_t aspace_id, unsigned long u_vaddr){
	unsigned long kernel_vaddr = userspace_vaddr2kenel_vaddr(aspace_id, u_vaddr);
	int i;
	for (i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
		if ((gdb_break[i].state == GDB_BP_SET) &&
				(gdb_break[i].bpt_addr == kernel_vaddr)) {
			gdb_break[i].state = GDB_BP_REMOVED;
			//printk("GDB: remove breakpoint at addr: %p %p\n", (void *) u_vaddr, (void *) kernel_vaddr);
			return 0;
		}
	}
	return -ENOENT;
}

int gdb_isremovedbreak(struct gdb_bkpt *gdb_break, id_t aspace_id, unsigned long u_vaddr){
	unsigned long kernel_vaddr = userspace_vaddr2kenel_vaddr(aspace_id, u_vaddr);
	int i;
	for (i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
		if ((gdb_break[i].state == GDB_BP_REMOVED) &&
			(gdb_break[i].bpt_addr == kernel_vaddr))
			return 1;
	}
	return 0;
}

int remove_all_break(struct gdb_bkpt *gdb_break){
	unsigned long addr;
	int error;
	int i;

	/* Clear memory breakpoints. */
	for (i = 0; i < GDB_MAX_BREAKPOINTS; i++) {
		if (gdb_break[i].state != GDB_BP_SET)
			continue;
		addr = gdb_break[i].bpt_addr;
		error = gdb_arch_remove_breakpoint(addr, gdb_break[i].saved_instr);
		if (error)
			return error;
		gdb_break[i].state = GDB_BP_REMOVED;
	}

	/* Clear hardware breakpoints. */
	if (arch_gdb_ops.remove_all_hw_break)
		arch_gdb_ops.remove_all_hw_break();

	return 0;
}

/*
 * All the functions that start with gdb_cmd are the various
 * operations to implement the handlers for the gdbserial protocol
 * where GDB is communicating with an external debugger
 */

/* Handle the 'g' get registers request */
static void gdb_cmd_getregs(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	if(ggs->gdb_usethread){//Hg0 or Hgpid
		//show_registers(task_pt_regs(&(ggs->gdb_usethread->arch)));
		pt_regs_to_gdb_regs(ggs->gdb_regs, task_pt_regs(&(ggs->gdb_usethread->arch)));
		gdb_mem2hex((char *)ggs->gdb_regs, ggs->remcom_out_buffer, GDB_NUM_GPREGBYTES);
	}else{//Hg-1 apply g command to all threads
		//error_packet(ggs->remcom_out_buffer, -EINVAL);
		int i = 0;
		struct task_struct *tsk;
		struct aspace *aspace = aspace_acquire(ggs->pid);
		list_for_each_entry(tsk, &aspace->task_list, aspace_link) {
			pt_regs_to_gdb_regs(ggs->gdb_regs, task_pt_regs(&(tsk->arch)));
			gdb_mem2hex((char *)ggs->gdb_regs, ggs->remcom_out_buffer + GDB_NUM_REGBYTES * i, GDB_NUM_REGBYTES);
			i++;
		}
		aspace_release(aspace);
	}
}
/* Handle the 'G' set registers request */
static void gdb_cmd_setregs(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	gdb_hex2mem(&ggs->remcom_in_buffer[1], (char *)ggs->gdb_regs, GDB_NUM_GPREGBYTES);
	if (!ggs->gdb_usethread 
			//|| ggs->gdb_usethread != TASK_PTR(gs->lwk_regs)
	) {
		error_packet(ggs->remcom_out_buffer, -EINVAL);
	} else {
		gdb_regs_to_pt_regs(ggs->gdb_regs, task_pt_regs(&(ggs->gdb_usethread->arch)));
		strcpy(ggs->remcom_out_buffer, "OK");
	}
}
/* Handle the '?' status packets */
static void gdb_cmd_status(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	/*
	 * We know that this packet is only sent
	 * during initial connect.  So to be safe,
	 * we clear out our breakpoints now in case
	 * GDB is reconnecting.
	 */
	remove_all_break(ggs->gdb_break);

	ggs->remcom_out_buffer[0] = 'S';
	pack_hex_byte(&ggs->remcom_out_buffer[1], gs->signo);
}

/* Handle the 'm' memory read bytes */
static void gdb_cmd_memread(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	char *ptr = &ggs->remcom_in_buffer[1];
	unsigned long length;
	unsigned long addr;
	int err = 0;

	if (gdb_hex2long(&ptr, &addr) > 0 && *ptr++ == ',' &&
		gdb_hex2long(&ptr, &length) > 0) {
		unsigned long kernel_vaddr = userspace_vaddr2kenel_vaddr(ggs->pid, addr);
		if(!kernel_vaddr){
			err = -EINVAL;    
		}else{
			err = gdb_mem2hex((char *) kernel_vaddr, ggs->remcom_out_buffer, length);
		}
		if (err) error_packet(ggs->remcom_out_buffer, err);
	} else {
		error_packet(ggs->remcom_out_buffer, -EINVAL);
	}
}

/* Handle the 'M' memory write bytes */
static void gdb_cmd_memwrite(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	int err = write_mem_msg(ggs, gs, 0);

	if (err)
		error_packet(ggs->remcom_out_buffer, err);
	else
		strcpy(ggs->remcom_out_buffer, "OK");
}

/* Handle the 'X' memory binary write bytes */
static void gdb_cmd_binwrite(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	int err = write_mem_msg(ggs, gs, 1);

	if (err)
		error_packet(ggs->remcom_out_buffer, err);
	else
		strcpy(ggs->remcom_out_buffer, "OK");
}

/* Handle the 'D' or 'k', detach or kill packets */
static void gdb_cmd_detachkill(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	int error;

	/* The detach case */
	if (ggs->remcom_in_buffer[0] == 'D') {
		error = remove_all_break(ggs->gdb_break);
		if (error < 0) {
			error_packet(ggs->remcom_out_buffer, error);
		} else {
			strcpy(ggs->remcom_out_buffer, "OK");
		}
		gdb_put_packet(ggs);
	} else {
		/*
		 * Assume the kill case, with no exit code checking,
		 * trying to force detach the debugger:
		 */
		remove_all_break(ggs->gdb_break);
    }
    //clear TF flag
    gs->lwk_regs->eflags &= ~TF_MASK;
    gdbio_detach(ggs->gdb_filep);
    gdb_htable_del(ggs);
    kmem_free(ggs);
}

/* Handle the 'R' reboot packets */
static int gdb_cmd_reboot(struct gdb_per_thread_state *gs){
	return 0;
}

/* Handle qXfer:features:read.  */
static int handle_qxfer_features (struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){ 
	extern const char *const xml_builtin[][2]; //target description 

	size_t total_len = strlen (xml_builtin[0][1]);
	size_t offset = 0, len = 0xFFF;
	if (offset > total_len)
		return -1;
	if (offset + len > total_len)
		len = total_len - offset;

	ggs->remcom_out_buffer[0] = 'l';
	memcpy (ggs->remcom_out_buffer + 1,  xml_builtin[0][1] + offset, len);

	return len;
}


/* Handle the 'q' query packets */
static void gdb_cmd_query(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	struct task_struct *thread;
	struct aspace *aspace;
	unsigned char thref[8];
	char *ptr;

	switch (ggs->remcom_in_buffer[1]) {
	case 's':
	case 'f':
		if (memcmp(ggs->remcom_in_buffer + 2, "ThreadInfo", 10)) {
			error_packet(ggs->remcom_out_buffer, -EINVAL);
			break;
		}

		ggs->remcom_out_buffer[0] = 'm';
		ptr = ggs->remcom_out_buffer + 1;

		aspace = aspace_acquire(ggs->pid);
		list_for_each_entry(thread, &aspace->task_list, aspace_link) {
			int_to_threadref(thref, thread->id);
			pack_threadid(ptr, thref);
			ptr += BUF_THREAD_ID_SIZE;
			*(ptr++) = ',';
		}
		aspace_release(aspace); 
		*(--ptr) = '\0';
		break;
	case 'C':
		/* Current thread id */
		strcpy(ggs->remcom_out_buffer, "QC");
		int_to_threadref(thref, TASK_PTR(gs->lwk_regs)->id);
		pack_threadid(ggs->remcom_out_buffer + 2, thref);
		break;
	case 'T':
		if (memcmp(ggs->remcom_in_buffer + 1, "TStatus", 7) == 0){
			break;    
		}

		if (memcmp(ggs->remcom_in_buffer + 1, "ThreadExtraInfo,", 16)) {
			error_packet(ggs->remcom_out_buffer, -EINVAL);
			break;
		}
		ptr = ggs->remcom_in_buffer + 17;
		gdb_hex2long(&ptr, &gs->threadid);
		thread = get_task(ggs->pid, gs->threadid); 
		if (!thread) {
			error_packet(ggs->remcom_out_buffer, -EINVAL);
			break;
		}
        
		gdb_mem2hex(thread->name, ggs->remcom_out_buffer, 16);
		break;
	case 'S':
		if(memcmp(ggs->remcom_in_buffer + 1, "Supported:", 10) == 0){
			static char str[33 + sizeof(int)]; 
			sprintf(str, "PacketSize=%04x;qXfer:features:read+", GDBIO_BUFFER_SIZE);
			memcpy(ggs->remcom_out_buffer, str, strlen(str));    
		}
		break;
	case 'X':
		if(memcmp(ggs->remcom_in_buffer + 1, "Xfer:features:read:", 19) == 0){
			handle_qxfer_features(ggs, gs);              
		}
		break;
	case 'A':
		if(memcmp(ggs->remcom_in_buffer + 1, "Attached", 8) == 0){
			ggs->remcom_out_buffer[0] = 1; //TODO 1 attach to an existing process; 0 start a new process 
		}
		break;
	}
}

/* Handle the 'H' task query packets */
static void gdb_cmd_task(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	struct task_struct *thread;
	char *ptr;

	switch (ggs->remcom_in_buffer[1]) {
	case 'g': //Hg0 Hg-1 Hgpid
		ptr = &ggs->remcom_in_buffer[2];
		gdb_hex2long(&ptr, &gs->threadid);
        
		if(gs->threadid == -1){//Hg-1
			thread = TASK_PTR(gs->lwk_regs);
		}else if(gs->threadid == 0){
			thread = TASK_PTR(gs->lwk_regs);
		}else{
			thread = get_task(ggs->pid, gs->threadid);
			if (!thread) {
				error_packet(ggs->remcom_out_buffer, -EINVAL);
				break;
			}
		}
        
		ggs->gdb_usethread = thread;
		strcpy(ggs->remcom_out_buffer, "OK");
		break;
	case 'c'://Hc-1 Hc0 Hcpid
		ptr = &ggs->remcom_in_buffer[2];
		gdb_hex2long(&ptr, &gs->threadid);
		if (!gs->threadid) {//Hc0 continue command apply to any threads
			thread = TASK_PTR(gs->lwk_regs);
		} else {
			if(gs->threadid == -1){//Hc-1 continue command apply to all threads
				thread = TASK_PTR(gs->lwk_regs);
			}else{
				thread = get_task(ggs->pid, gs->threadid);
				if (!thread) {
					error_packet(ggs->remcom_out_buffer, -EINVAL);
					break;
				}
			}
		}
		ggs->gdb_contthread = thread;
		strcpy(ggs->remcom_out_buffer, "OK");
		break;
	}
}

/* Handle the 'T' thread query packets */
static void gdb_cmd_thread(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	char *ptr = &ggs->remcom_in_buffer[1];
	struct task_struct *thread;

	gdb_hex2long(&ptr, &gs->threadid);
	thread = get_task(ggs->pid, gs->threadid);
	if (thread)
		strcpy(ggs->remcom_out_buffer, "OK");
	else
		error_packet(ggs->remcom_out_buffer, -EINVAL);
}

/* Handle the 'z' or 'Z' breakpoint remove or set packets */
static void gdb_cmd_break(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	/*
	 * Since GDB-5.3, it's been drafted that '0' is a software
	 * breakpoint, '1' is a hardware breakpoint, so let's do that.
	 */
	char *bpt_type = &ggs->remcom_in_buffer[1];
	char *ptr = &ggs->remcom_in_buffer[2];
	unsigned long addr;
	unsigned long length;
	int error = 0;

	if (arch_gdb_ops.set_hw_breakpoint && *bpt_type >= '1') {
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
	if (*bpt_type == '1' && !(arch_gdb_ops.flags & GDB_HW_BREAKPOINT))
		/* Unsupported. */
		return;

	if (*(ptr++) != ',') {
		error_packet(ggs->remcom_out_buffer, -EINVAL);
		return;
	}
	if (!gdb_hex2long(&ptr, &addr)) {
		error_packet(ggs->remcom_out_buffer, -EINVAL);
		return;
	}
	if (*(ptr++) != ',' ||
		!gdb_hex2long(&ptr, &length)) {
		error_packet(ggs->remcom_out_buffer, -EINVAL);
		return;}

	if (ggs->remcom_in_buffer[0] == 'Z' && *bpt_type == '0')
		error = gdb_set_sw_break(ggs->gdb_break, ggs->pid, addr);
	else if (ggs->remcom_in_buffer[0] == 'z' && *bpt_type == '0')
		error = gdb_remove_sw_break(ggs->gdb_break, ggs->pid, addr);
	else if (ggs->remcom_in_buffer[0] == 'Z')
		error = arch_gdb_ops.set_hw_breakpoint(addr, (int)length, *bpt_type);
	else if (ggs->remcom_in_buffer[0] == 'z')
		error = arch_gdb_ops.remove_hw_breakpoint(addr, (int) length, *bpt_type);

	if (error == 0)
		strcpy(ggs->remcom_out_buffer, "OK");
	else
		error_packet(ggs->remcom_out_buffer, error);
}

/* Handle the 'C' signal / exception passing packets */
static int gdb_cmd_exception_pass(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs){
	/* C09 == pass exception
	 * C15 == detach gdb, pass exception
	 */
	if (ggs->remcom_in_buffer[1] == '0' && ggs->remcom_in_buffer[2] == '9') {
		gs->pass_exception = 1;
		ggs->remcom_in_buffer[0] = 'c';
	} else if (ggs->remcom_in_buffer[1] == '1' && ggs->remcom_in_buffer[2] == '5') {
		gs->pass_exception = 1;
		ggs->remcom_in_buffer[0] = 'D';
		remove_all_break(ggs->gdb_break);
		ggs->gdb_connected = 0;
		return 1;
	} else {
		error_packet(ggs->remcom_out_buffer, -EINVAL);
		return 0;
	}

	/* Indicate fall through */
	return -1;
}

/*
 * This function performs all gdbserial command procesing
 */
static int gdb_serial_stub(struct gdb_per_process_state *ggs, struct gdb_per_thread_state *gs)
{
	int error = 0;
	int tmp;
         
	memset(ggs->remcom_out_buffer, 0, sizeof(ggs->remcom_out_buffer));    
	if (ggs->gdb_connected) {//T reply for c and s command upon a break is encountered
		unsigned char thref[8];
		char *ptr;

		ptr = ggs->remcom_out_buffer;
		*ptr++ = 'T';
		ptr = pack_hex_byte(ptr, gs->signo);
		ptr += strlen(strcpy(ptr, "thread:"));
		int_to_threadref(thref, ggs->gdb_usethread->id);
		ptr = pack_threadid(ptr, thref);
		*ptr++ = ';';
		gdb_put_packet(ggs);
	}
     
	while (1) {
		error = 0;

		/* Clear the out buffer. */
		memset(ggs->remcom_out_buffer, 0, sizeof(ggs->remcom_out_buffer));
		gdb_get_packet(ggs);
          
		switch (ggs->remcom_in_buffer[0]) {
		case '?': /* gdbserial status */
			gdb_cmd_status(ggs, gs);
			break;
		case 'g': /* return the value of the CPU registers */
			gdb_cmd_getregs(ggs, gs);
			break;
		case 'G': /* set the value of the CPU registers - return OK */
			gdb_cmd_setregs(ggs, gs);
			break;
		case 'm': /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
			gdb_cmd_memread(ggs, gs);
			break;
		case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA..AA */
			gdb_cmd_memwrite(ggs, gs);
			break;
		case 'X': /* XAA..AA,LLLL: Write LLLL bytes at address AA..AA */
			gdb_cmd_binwrite(ggs, gs);
			break;
			/* kill or detach. GDB should treat this like a
			 * continue.
			 */
		case 'D': /* Debugger detach */
		case 'k': /* Debugger detach via kill */
			gdb_cmd_detachkill(ggs, gs);
			goto default_handle;
		case 'R': /* Reboot */
			if (gdb_cmd_reboot(gs))
				goto default_handle;
			break;
		case 'q': /* query command */
			gdb_cmd_query(ggs, gs);
			break;
		case 'H': /* task related */
			gdb_cmd_task(ggs, gs);
			break;
		case 'T': /* Query thread status */
			gdb_cmd_thread(ggs, gs);
			break;
		case 'z': /* Break point remove */
		case 'Z': /* Break point set */
			gdb_cmd_break(ggs, gs);
			break;
		case 'C': /* Exception passing */
			tmp = gdb_cmd_exception_pass(ggs, gs);
			if (tmp > 0)
				goto default_handle;
			if (tmp == 0)
				break;
			/* Fall through on tmp < 0 */
		case 'c': /* Continue packet */
		case 's': /* Single step packet */
			/*if (ggs->gdb_contthread 
				&& (ggs->gdb_contthread->aspace->id != TASK_PTR(gs->lwk_regs)->aspace_id)) {
				error_packet(ggs->remcom_out_buffer, -EINVAL);
				break;
			}*/
    
			gdb_activate_sw_breakpoints(ggs->gdb_break);
			/* Fall through to default processing */
		default:
default_handle:
			error = gdb_arch_handle_exception(gs->ex_vector,
					gs->signo,
					gs->err_code,
					gs->lwk_regs,
					ggs->remcom_in_buffer,
					ggs->remcom_out_buffer,
					&ggs->gdb_cpu_doing_single_step);
			/*
			 * Leave cmd processing on error, detach,
			 * kill, continue, or single step.
			 */
			if(error >= 0 || ggs->remcom_in_buffer[0] == 'D' 
				|| ggs->remcom_in_buffer[1] == 'k'){
				error = 0;
				goto gdb_exit;
			} 
		}
		/* reply to the request */
		gdb_put_packet(ggs);
    }

gdb_exit:
	if (gs->pass_exception)
		error = -1;
	if(ggs->remcom_in_buffer[0] == 's'){
		gdb_stop_or_resume_single_task(TASK_PTR(gs->lwk_regs), GDB_RESUME_TASK);
	}else{
		id_t pid = TASK_PTR(gs->lwk_regs)->aspace->id;
		gdb_stop_or_resume_all(get_task(pid, pid), GDB_RESUME_TASK);
   	}
	return error;
}
/*
void gdb_roundup_cpus(unsigned long flags)
{
	if (gdb_stop_cpus)
        	gdb_nmi_cpus();
}
*/
/*
 * gdb_handle_exception() - main entry point from a userspace int 3 exception
 *
 * Locking hierarchy:
 *	interface locgs, if any (begin_session)
 *	gdb lock (gdb_active)
 */
int gdb_handle_exception(int evector, int signo, int ecode, struct pt_regs *regs){
	struct gdb_per_thread_state gdb_var;
	struct gdb_per_thread_state *gs = &gdb_var;
	unsigned long flags;
	int error = 0;
	//int i; 
	int cpu;

	gs->cpu			= raw_smp_processor_id();
	gs->ex_vector		= evector;
	gs->signo		= signo;
	gs->ex_vector		= evector;
	gs->err_code		= ecode;
	gs->lwk_regs		= regs;
	gs->pass_exception	= 0;
    
	struct task_struct *task = TASK_PTR(regs); 
	struct gdb_per_process_state *ggs = gdb_htable_lookup(task->aspace->id);
	if(!ggs){
		printk("GDB Error: no corresponding gdb global state for process(%d)\n", ggs->pid);
		return -1;    
	}

acquirelock:
	/*
	 * Interrupts will be restored by the 'trap return' code, except when
	 * single stepping.
	 */
	local_irq_save(flags);

	cpu = raw_smp_processor_id();

	/*
	* Acquire the gdb_active lock:
	 */
	while (atomic_cmpxchg(&(ggs->gdb_active), -1, cpu) != -1)
		cpu_relax();
    

	/*
	 * Do not start the debugger connection on this CPU if the last
	 * instance of the exception handler wanted to come into the
	 * debugger on a different CPU via a single step
	 */
	if (atomic_read(&(ggs->gdb_cpu_doing_single_step)) != -1 &&
		atomic_read(&(ggs->gdb_cpu_doing_single_step)) != cpu) {

		atomic_set(&ggs->gdb_active, -1);
		local_irq_restore(flags);

		goto acquirelock;
	}

	if (!ggs->gdb_connected) {
		printk("GDB resuming because I/O not ready.\n");
		error = -1;
		goto gdb_restore; /* No I/O connection, so resume the system */
	}
    
	/*
	 * Don't enter if we have hit a removed breakpoint.
	 */
	if (gdb_skipexception(ggs->gdb_break, ggs->pid, gs->ex_vector, gs->lwk_regs))
		goto gdb_restore;

	/* No need to signal other CPUs to enter gdb_wait???
	 * Get the passive CPU lock which will hold all the non-primary
	 * CPU in a spin state while the debugger is active
	 */
	/*if (!gdb_single_step || !ggs->gdb_contthread) {
		for (i = 0; i < NR_CPUS; i++)
			atomic_set(&passive_cpu_wait[i], 1);
	}*/

	/* Signal the other CPUs to enter gdb_wait() */
	/*if (!gdb_single_step || !ggs->gdb_contthread)
		gdb_roundup_cpus(flags);
	*/

	/*
	 * spin_lock code is good enough as a barrier so we don't
	 * need one here:
	 */
	//atomic_set(&cpu_in_gdb[gs->cpu], 1);

	/*
	 * Wait for the other CPUs to be notified and be waiting for us:
	 */
	/*if (gdb_stop_cpus) {
		for_each_online_cpu(i) {
			while (!atomic_read(&cpu_in_gdb[i]))
				cpu_relax();
		}
	}*/

	/*
	 * At this point the primary processor is completely
	 * in the debugger and all secondary CPUs are quiescent
	 */
	gdb_post_primary_code(gs->lwk_regs, gs->ex_vector, gs->err_code);

	gdb_stop_or_resume_all_except(get_task(ggs->pid, ggs->pid), task->id, GDB_STOP_TASK);	
	gdb_disable_hw_debug(gs->lwk_regs);
	gdb_deactivate_sw_breakpoints(ggs->gdb_break);
	//gdb_single_step = 0;
	ggs->gdb_contthread = NULL;
	ggs->gdb_usethread   = 	task;
	/* Talk to debugger with gdbserial protocol */
	error = gdb_serial_stub(ggs, gs);

	/*atomic_set(&cpu_in_gdb[gs->cpu], 0);
	if (!gdb_single_step || !ggs->gdb_contthread) {
		for (i = NR_CPUS-1; i >= 0; i--)
			atomic_set(&passive_cpu_wait[i], 0);
		if (gdb_stop_cpus) {
			for_each_online_cpu(i) {
				while (atomic_read(&cpu_in_gdb[i]))
					cpu_relax();
			}
		}
	}*/

gdb_restore:
	/* Free gdb_active */
	atomic_set(&(ggs->gdb_active), -1);
	local_irq_restore(flags);
	
	return error;
}

/*int gdb_nmicallback(int cpu, void *regs)
{
	if (!atomic_read(&cpu_in_gdb[cpu]) &&
			atomic_read(&gdb_active) != cpu) {
		gdb_wait((struct pt_regs *)regs);
		return 0;
	}
	return 1;
}
*/

/*
 * Attach to a running task(it can be either a process or a thread)
 */
static int __gdb_attach(void *priv){ 
	struct task_struct *task = (struct task_struct *) priv;
	struct gdb_per_process_state * ggs = gdb_htable_lookup(task->aspace->id);

	gdb_stop_or_resume_all(task, GDB_STOP_TASK);    
	printk("GDB: attach to task(pid=%d, tid=%d) successfully, waiting for gdb client to connect\n", task->aspace->id, task->id); 
	//printk("GDB: attach to task(%d, %d) successfully and wait for gdb client to connect via %s\n", task->aspace->id, task->id, ggs->gdb_filep->inode->name);
	while(gdbio_get_char(ggs->gdb_filep) != '+');

	ggs->gdb_connected = 1;

	struct pt_regs *regs = task_pt_regs(&task->arch); 
	return gdb_handle_exception(INT3_VECTOR, 0x05, 0, regs);
}

static int gdb_pre_attach(struct task_struct *task, char *gdb_cons){
	if(!atomic_read(&htable_inited)){
		printk("GDB Error: gdb module haven't been inited yet\n");
		return -1;    
	}

	struct pt_regs *regs = task_pt_regs(&task->arch);
	if(!user_mode(regs)){
		printk("GDB Error: permission deny! Task(pid=%d, tid=%d) is running in kernel space!\n", task->aspace->id, task->id);
		return -1;    
	}

	/*if(task->aspace->id == INIT_ASPACE_ID){
		printk("GDB Error: permission deny! gdb is not allowed to debug init_task\n");
		return -1;    
	}*/

	struct gdb_per_process_state * ggs = gdb_htable_lookup(task->aspace->id);
	if(ggs){
		printk("GDB Error: process(pid=%d) is being debuged by someone else\n", task->aspace->id);    
		return -1;
	}

	struct file *gdb_filep = gdbio_init(gdb_cons);
	if(!gdb_filep){
		return -1;    
	}

	ggs = kmem_alloc(sizeof(struct gdb_per_process_state));
	if(!ggs){
		printk("GDB Error: allocate memory for gdb_per_process_state failed\n");
		rm_gdb_fifo(gdb_filep);
		return -1;    
	}
	ggs->pid = task->aspace->id; 
	ggs->gdb_filep = gdb_filep;
	ggs->gdb_connected = 0;
	atomic_set(&ggs->gdb_active, -1);
	atomic_set(&ggs->gdb_cpu_doing_single_step, -1);
	int i;
	for(i=0; i<GDB_MAX_BREAKPOINTS; i++){
		ggs->gdb_break[i].state = GDB_BP_UNDEFINED; 
	}
	gdb_htable_add(ggs);

	if(atomic_read(&pt_regs_task_offset) == -1){
		int offset = (int) ((void *)regs - (void *)task); 
		atomic_set(&pt_regs_task_offset, offset);
		//printk("GDB: %d\n", atomic_read(&pt_regs_task_offset));
	}

	return 0;
}

/*
 * Attach to a running process
 * task->aspace->id is also the pid of the process as well as the id of the process task_struct.
 */
int gdb_attach_process(id_t pid, char *gdb_cons){
	printk("GDB: attaching to process(%d)\n", pid);
	struct task_struct *task = get_task(pid, pid);
	if(!task){
		printk("GDB Error: process(%d) doesn't exist\n", pid);
		return -1;
	} 

	if(gdb_pre_attach(task, gdb_cons) == -1){
		return -1;
	}

	kthread_run(__gdb_attach, task, "GDB-STUB-THREAD");
	return 0;
}
/*
 * Attach to a single running thread
 * */
int gdb_attach_thread(id_t pid, id_t tid, char *gdb_cons){
	printk("GDB: attaching to thread(pid=%d, tid=%d)\n", pid, tid);
	struct task_struct *task = get_task(pid, tid);
	if(!task){
		printk("GDB Error: thread(pid=%d, tid=%d) doesn't exist\n", pid, tid);
		return -1;
	}

	if(gdb_pre_attach(task, gdb_cons) == -1){
		return -1;
	}

	kthread_run(__gdb_attach, task, "GDB-STUB-THREAD");
	return 0;
}

static int __init gdb_module_init(void){
	gdb_htable = htable_create(
		GDB_HTABLE_ORDER,
		offsetof(struct gdb_per_process_state, pid),
		offsetof(struct gdb_per_process_state, ht_link),
		gdb_hash_pid,
		gdb_key_compare
	);
	if(!gdb_htable){
		printk("GDB Error: init gdb module failed\n");
		return -1;
	}
	spin_lock_init(&htable_lock);
	atomic_set(&htable_inited, 1);
	//printk("GDB: init gdb module successfully\n");

	return 0;
}

static void __exit gdb_module_deinit(void){
	//printk("GDB: deinit gdbio module\n");
	spin_lock(&htable_lock);
	if(gdb_htable && htable_empty(gdb_htable)){
		struct gdb_per_process_state *ggs;
		struct htable_iter iter = htable_iter( gdb_htable );
		while( (ggs = htable_next( &iter )) ){
			kmem_free(ggs);
		}

		htable_destroy(gdb_htable);
		gdb_htable = NULL;
	}
	spin_unlock(&htable_lock);
	atomic_set(&htable_inited, 0);
}

#ifdef CONFIG_PALACIOS_GDB
DRIVER_INIT("module", gdb_module_init);
DRIVER_EXIT(gdb_module_deinit);
#endif

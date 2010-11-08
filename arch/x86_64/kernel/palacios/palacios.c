/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it under the terms of the GNU General Public License
 * Version 2 (GPLv2).  The accompanying COPYING file contains the
 * full text of the license.
 */

#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/pmem.h>
#include <lwk/string.h>
#include <lwk/cpuinfo.h>
#include <lwk/driver.h>
#include <lwk/kthread.h>
#include <arch/page.h>
#include <arch/ptrace.h>
#include <arch/apic.h>
#include <arch/idt_vectors.h>
#include <arch/proto.h>
#include "palacios.h"
#include <lwk/signal.h>
#include <lwk/xcall.h>
#include <lwk/interrupt.h>
#include <lwk/sched.h>
#include <lwk/cpuinfo.h>
#include <arch/io.h>
#include <arch/unistd.h>
#include <arch/vsyscall.h>
#include <arch/io_apic.h>



/**
 * Global guest state... only one guest is supported currently.
 */
static struct v3_vm_info * g_vm_guest = NULL;
static struct v3_vm_info * irq_to_guest_map[NUM_IDT_ENTRIES];
static paddr_t guest_iso_start;
static size_t guest_iso_size;

/**
 * Sends a keyboard key press event to Palacios for handling.
 */
void
send_key_to_palacios(
	unsigned char		status,
	unsigned char		scan_code
)
{
	if (!g_vm_guest)
		return;

	struct v3_keyboard_event event = {
		.status    = status,
		.scan_code = scan_code,
	};

	v3_deliver_keyboard_event(g_vm_guest, &event);
}

/**
 * Sends a mouse event to Palacios for handling.
 */
void
send_mouse_to_palacios(
	unsigned char		packet[3]
)
{
	if (!g_vm_guest)
		return;

	struct v3_mouse_event event;
	memcpy(event.data, packet, 3);

	v3_deliver_mouse_event(g_vm_guest, &event);
}

/**
 * Sends a timer tick event to Palacios for handling.
 */
void
send_tick_to_palacios(
	unsigned int		period_us
)
{
	if (!g_vm_guest)
		return;

	struct v3_timer_event event = {
		.period_us = period_us,
	};

	v3_deliver_timer_event(g_vm_guest, &event);
}

/**
 * Prints a message to the console.
 */
static void
palacios_print(
	const char *		fmt,
	...
)
{
	va_list ap;
	va_start(ap, fmt);
	vprintk(fmt, ap);
	va_end(ap);
}

/**
 * Allocates a contiguous region of pages of the requested size.
 * Returns the physical address of the first page in the region.
 */
static void *
palacios_allocate_pages(
	int			num_pages,
	unsigned int		alignment	// must be power of two
)
{
	struct pmem_region result;
	int status;

	/* Allocate from the user-managed physical memory pool */
	status = pmem_alloc_umem(num_pages * PAGE_SIZE, alignment, &result);
	if (status)
		return NULL;

	/* Return the physical address of the region */
	return (void *) result.start;
}

/**
 * Frees a page previously allocated via palacios_allocate_page().
 * Note that palacios_allocate_page() can allocate multiple pages with
 * a single call while palacios_free_page() only frees a single page.
 */
static void
palacios_free_page(
	void *			page_paddr
) 
{
	struct pmem_region	query;
	struct pmem_region	result;
	int 			status;

	pmem_region_unset_all(&query);

	query.start		= (uintptr_t) page_paddr;
	query.end		= (uintptr_t) page_paddr + PAGE_SIZE;
	query.allocated		= true;
	query.allocated_is_set	= true;

	status = pmem_query(&query, &result);
	if (status)
		panic("Freeing page %p failed! query status=%d",
		      page_paddr, status);

	result.allocated = false;
	status = pmem_update(&result);
	if (status)
		panic("Failed to free page %p! (status=%d)",
		      page_paddr, status);
}

/**
 * Allocates 'size' bytes of kernel memory.
 * Returns the kernel virtual address of the memory allocated.
 */
static void *
palacios_alloc(
	unsigned int		size
)
{
	return kmem_alloc(size);
}

/**
 * Frees memory that was previously allocated by palacios_alloc().
 */
static void
palacios_free(
	void *			addr
)
{
	return kmem_free(addr);
}

/**
 * Converts a kernel virtual address to the corresponding physical address.
 */
static void *
palacios_vaddr_to_paddr(
	void *			vaddr
)
{
	return (void *) __pa(vaddr);
}

/**
 * Converts a physical address to the corresponding kernel virtual address.
 */
static void *
palacios_paddr_to_vaddr(
	void *			paddr
)
{
	return (void *) __va(paddr);
}

/**
 * Runs a function on the specified CPU.
 */
static void 
palacios_xcall(
	int			cpu_id, 
	void			(*fn)(void *arg),
	void *			arg
)
{
	cpumask_t cpu_mask;

	cpus_clear(cpu_mask);
	cpu_set(cpu_id, cpu_mask);

	printk(KERN_DEBUG
		"Palacios making xcall to cpu %d from cpu %d.\n",
		cpu_id, current->cpu_id);

	xcall_function(cpu_mask, fn, arg, 1);
}

/**
 * Starts a kernel thread on the specified CPU.
 */
static void *
palacios_start_thread_on_cpu(
	int			cpu_id, 
	int			(*fn)(void * arg), 
	void *			arg, 
	char *			thread_name
)
{
	return kthread_create_on_cpu(cpu_id, fn, arg, thread_name);
}

/**
 * Returns the CPU ID that the caller is running on.
 */
static unsigned int 
palacios_get_cpu(void) 
{
	return this_cpu;
}

/**
 * Interrupts the physical CPU corresponding to the specified logical guest cpu.
 * If (vector == 0) then it is just an interrupt with no effect, this merely kicks the 
 * core out of the guest context
 *
 * NOTE: 
 * This is dependent on the implementation of xcall_reschedule().  Currently
 * xcall_reschedule does not explicitly call schedule() on the destination CPU,
 * but instead relies on the return to user space to handle it. Because
 * palacios is a kernel thread schedule will not be called, which is correct.
 * We should have a default palacios IRQ that just handles the IPI and returns immediately
 * with no side effects.
 */
static void
palacios_interrupt_cpu(
	struct v3_vm_info*	vm, 
	int			cpu_id,
	int                     vector
)
{
	if (cpu_id != current->cpu_id) {
		if (vector == 0) 
			xcall_reschedule(cpu_id);
		else 
			lapic_send_ipi(cpu_id, vector);
	}
}

/**
 * Dispatches an interrupt to Palacios for handling.
 */
static void
palacios_dispatch_interrupt(
	struct pt_regs *	regs,
	unsigned int		vector
) 
{
	struct v3_interrupt intr = {
		.irq		= vector,
		.error		= regs->orig_rax,
		.should_ack	= 1,
	};

	v3_deliver_irq(irq_to_guest_map[vector], &intr);
}

/**
 * Instructs the kernel to forward the specified IRQ to Palacios.
 */
static int
palacios_hook_interrupt(
	struct v3_vm_info *	vm,
	unsigned int		vector
)
{
	if (irq_to_guest_map[vector]) {
		printk(KERN_WARNING
		       "%s: Interrupt vector %u is already hooked.\n",
		       __func__, vector);
		return -1;
	}

	printk(KERN_DEBUG
	       "%s: Hooking interrupt vector %u to vm %p.\n",
	       __func__, vector, vm);

	irq_to_guest_map[vector] = vm;

	/*
	 * NOTE: Normally PCI devices are supposed to be level sensitive,
	 *       but we need them to be edge sensitive so that they are
	 *       properly latched by Palacios.  Leaving them as level
	 *       sensitive would lead to an interrupt storm.
	 */
	ioapic_set_trigger_for_vector(vector, ioapic_edge_sensitive);

	set_idtvec_handler(vector, palacios_dispatch_interrupt);

	return 0;
}

/**
 * Acknowledges an interrupt.
 */
static int
palacios_ack_interrupt(
	int			vector
) 
{
	lapic_ack_interrupt();
	return 0;
}
  
/**
 * Returns the CPU frequency in kilohertz.
 */
static unsigned int
palacios_get_cpu_khz(void) 
{
	return cpu_info[0].arch.cur_cpu_khz;
}

/**
 * Yield the CPU so other host OS tasks can run.
 */
static void
palacios_yield_cpu(void)
{
	schedule();
}

/**
 * Creates a kernel thread.
 */
static void 
palacios_start_kernel_thread(
	int (*fn)		(void *arg),
	void *			arg,
	char *			thread_name)
{
    kthread_create(fn, arg, thread_name);
}

/**
 * Allocates a mutex.
 * Returns NULL on failure.
 */
static void *
palacios_mutex_alloc(void)
{
	spinlock_t *lock = kmem_alloc(sizeof(spinlock_t));
	if (lock)
		spin_lock_init(lock);
	return lock;
}

/**
 * Frees a mutex.
 */
static void
palacios_mutex_free(
	void *			mutex
) 
{
	kmem_free(mutex);
}

/**
 * Locks a mutex.
 */
static void 
palacios_mutex_lock(
	void *			mutex, 
	int			must_spin
)
{
	spin_lock((spinlock_t *)mutex);
}

/**
 * Unlocks a mutex.
 */
static void 
palacios_mutex_unlock(
	void *			mutex
) 
{
	spin_unlock((spinlock_t *)mutex);
}

/**
 * Structure used by the Palacios hypervisor to interface with the host kernel.
 */
struct v3_os_hooks palacios_os_hooks = {
	.print			= palacios_print,
	.allocate_pages		= palacios_allocate_pages,
	.free_page		= palacios_free_page,
	.malloc			= palacios_alloc,
	.free			= palacios_free,
	.vaddr_to_paddr		= palacios_vaddr_to_paddr,
	.paddr_to_vaddr		= palacios_paddr_to_vaddr,
	.hook_interrupt		= palacios_hook_interrupt,
	.ack_irq		= palacios_ack_interrupt,
	.get_cpu_khz		= palacios_get_cpu_khz,
	.start_kernel_thread    = palacios_start_kernel_thread,
	.yield_cpu		= palacios_yield_cpu,
	.mutex_alloc		= palacios_mutex_alloc,
	.mutex_free		= palacios_mutex_free,
	.mutex_lock		= palacios_mutex_lock, 
	.mutex_unlock		= palacios_mutex_unlock,
	.get_cpu		= palacios_get_cpu,
	.interrupt_cpu		= palacios_interrupt_cpu,
	.call_on_cpu		= palacios_xcall,
	.start_thread_on_cpu	= palacios_start_thread_on_cpu,
};

/**
 * Starts a guest operating system.
 */
static int
palacios_run_guest(void *arg)
{
	unsigned int mask;
	struct v3_vm_info * vm_info = v3_create_vm((void *) __va(guest_iso_start), NULL, NULL);
	
	if (!vm_info) {
		printk(KERN_ERR "Could not create guest context\n");
		return -1;
	}

	g_vm_guest = vm_info;

	printk(KERN_INFO "Starting Guest OS...\n");

	// set the mask to inclue all available CPUs
	// we assume we will start on CPU 0
	mask=~((((signed int)1<<(sizeof(unsigned int)*8-1))>>(sizeof(unsigned int)*8-1))<<cpus_weight(cpu_online_map));

	return v3_start_vm(vm_info, mask);
}

/**
 * Kicks off a kernel thread to start and manage a guest operating system.
 */
static int
sys_v3_start_guest(
	paddr_t			iso_start,
	size_t			iso_size
)
{
	if (current->uid != 0)
		return -EPERM;

	guest_iso_start = iso_start;
	guest_iso_size  = iso_size;

	return palacios_run_guest(0);
}

/**
 * Direct keyboard interrupts to the Palacios hypervisor.
 */
static irqreturn_t
palacios_keyboard_interrupt(
	int			vector,
	void *			unused
)
{
	const uint8_t KB_STATUS_PORT = 0x64;
	const uint8_t KB_DATA_PORT   = 0x60;
	const uint8_t KB_OUTPUT_FULL = 0x01;

	uint8_t status = inb(KB_STATUS_PORT);

	if ((status & KB_OUTPUT_FULL) == 0)
		return IRQ_NONE;

	uint8_t key = inb(KB_DATA_PORT);
	send_key_to_palacios(status, key);

	return IRQ_HANDLED;
}

/**
 * Initialize the Palacios hypervisor.
 */
static int
palacios_init(void)
{

	printk(KERN_INFO "---- Initializing Palacios hypervisor support\n");
	printk(KERN_INFO "cpus_weight(cpu_online_map)=0x%x\n",cpus_weight(cpu_online_map));

	Init_V3(&palacios_os_hooks, cpus_weight(cpu_online_map));

	irq_request(
		IRQ1_VECTOR,
		&palacios_keyboard_interrupt,
		0,
		"keyboard",
		NULL
	);

	syscall_register(__NR_v3_start_guest, (syscall_ptr_t) sys_v3_start_guest);

	return 0;
}

DRIVER_INIT( "module", palacios_init );

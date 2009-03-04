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
#include <lwk/pmem.h>
#include <lwk/string.h>
#include <lwk/cpuinfo.h>
#include <lwk/driver.h>
#include <arch/page.h>
#include <arch/ptrace.h>
#include <arch/apic.h>
#include <arch/idt_vectors.h>
#include <arch/proto.h>
#include "palacios.h"
#include <lwk/signal.h>
#include <lwk/interrupt.h>
#include <lwk/sched.h>
#include <arch/io.h>
#include <arch/unistd.h>
#include <arch/vsyscall.h>

/**
 * Location of the ROM BIOS and VGA BIOS used by Palacios guests.
 * These are kernel virtual pointers to the memory.
 */
extern uint8_t rombios_start[], rombios_end[];
extern uint8_t vgabios_start[], vgabios_end[];

/**
 * Palacios management and control function pointers.
 */
static struct v3_ctrl_ops v3_ops = {};

/**
 * Global guest state... only one guest is supported currently.
 */
static struct guest_info * g_vm_guest = NULL;
static struct guest_info * irq_to_guest_map[NUM_IDT_ENTRIES];
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
	int			num_pages
)
{
	struct pmem_region result;
	int status;
  
	/* Allocate from the user-managed physical memory pool */
	status = pmem_alloc_umem(num_pages * PAGE_SIZE, PAGE_SIZE, &result);
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
 * Dispatches an interrupt to Palacios for handling.
 */
static void
palacios_dispatch_interrupt(
	struct pt_regs *	regs,
	unsigned int		vector
) 
{
	struct v3_interrupt intr = {
		.irq		= vector - IRQ0_VECTOR,  /* TODO: correct? */
		.error		= regs->orig_rax,
		.should_ack	= 0,
	};

	printk(KERN_DEBUG "%s: Delivering IRQ %u (vector %u) to guest.\n",
	                  __func__, intr.irq, vector);
	v3_deliver_irq(irq_to_guest_map[intr.irq], &intr);
}

/**
 * Instructs the kernel to forward the specified IRQ to Palacios.
 */
static int
palacios_hook_interrupt(
	struct guest_info *	vm,
	unsigned int		irq
)
{
	if (irq_to_guest_map[irq]) {
		printk(KERN_WARNING
		       "%s: IRQ %d is already hooked.\n", __func__, irq);
		return -1;
	}

	printk(KERN_DEBUG "%s: Hooking IRQ %d to %p.\n", __func__, irq, vm);
	irq_to_guest_map[irq] = vm;
	set_idtvec_handler(irq, palacios_dispatch_interrupt);

	return 0;
}

/**
 * Acknowledges an interrupt.
 */
static int
palacios_ack_irq(
	int			irq
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

static void 
palacios_create_thread(int (*fn)(void * arg), void * arg, char * thread_name)
{
    kthread_create(fn, arg, thread_name);
}


/**
 * Structure used by the Palacios hypervisor to interface with the host kernel.
 */
struct v3_os_hooks palacios_os_hooks = {
	.print_debug		= palacios_print,
	.print_info		= palacios_print,
	.print_trace		= palacios_print,
	.allocate_pages		= palacios_allocate_pages,
	.free_page		= palacios_free_page,
	.malloc			= palacios_alloc,
	.free			= palacios_free,
	.vaddr_to_paddr		= palacios_vaddr_to_paddr,
	.paddr_to_vaddr		= palacios_paddr_to_vaddr,
	.hook_interrupt		= palacios_hook_interrupt,
	.ack_irq		= palacios_ack_irq,
	.get_cpu_khz		= palacios_get_cpu_khz,
	.start_kernel_thread    = palacios_create_thread,
	.yield_cpu		= palacios_yield_cpu,
};

/**
 * Starts a guest operating system.
 */
static int
palacios_run_guest(void *arg)
{
	struct v3_vm_config vm_config = {
		.rombios		= rombios_start,
		.rombios_size		= rombios_end - rombios_start,
		.vgabios		= vgabios_start,
		.vgabios_size		= vgabios_end - vgabios_start,
		.mem_size		= (128 * 1024 * 1024),
		.use_ramdisk		= 1,
		.ramdisk		= (void *) guest_iso_start,
		.ramdisk_size		= guest_iso_size,
        };

	struct guest_info * vm_info = v3_ops.allocate_guest();

	v3_ops.config_guest(vm_info, &vm_config);

	v3_ops.init_guest(vm_info);
	g_vm_guest = vm_info;

	printk(KERN_INFO "Starting Guest OS...\n");
	v3_ops.start_guest(vm_info);

	return 0;
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

	kthread_create(palacios_run_guest, NULL, "guest_os");

	return 0;
}

/**
 * Direct keyboard interrupts to the Palacios hypervisor.
 */
static irqreturn_t
palacios_keyboard_interrupt(
	unsigned int		vector,
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
static void
palacios_init(void)
{
	printk(KERN_INFO "---- Initializing Palacios hypervisor support\n");

	Init_V3(&palacios_os_hooks, &v3_ops);

	irq_request(
		IRQ1_VECTOR,
		&palacios_keyboard_interrupt,
		0,
		"keyboard",
		NULL
	);

	syscall_register(__NR_v3_start_guest, (syscall_ptr_t) sys_v3_start_guest);
}

driver_init( "module", palacios_init );


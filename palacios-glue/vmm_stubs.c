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
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <lwk/palacios.h>
#include <lwk/types.h>
#include <lwk/pmem.h>
#include <lwk/string.h>
#include <lwk/cpuinfo.h>
#include <lwk/kernel.h>
#include <arch/page.h>
#include <arch/ptrace.h>
#include <arch/apic.h>

struct guest_info * g_vm_guest = NULL;
struct guest_info * irq_to_guest_map[256];



void
v3vee_init_stubs( void )
{
	memset(irq_to_guest_map, 0, sizeof(irq_to_guest_map) );
}


static void *
kitten_pa_to_va(void *ptr)
{
	return (void*) __va(ptr);
}


static void *
kitten_va_to_pa(
	void *			ptr
)
{
	return (void*) __pa(ptr);
}


static void *
Allocate_VMM_Pages(
	int			num_pages
) 
{
	struct pmem_region result;
  
	int rc = pmem_alloc_umem( num_pages*PAGE_SIZE,PAGE_SIZE, &result );
	if( rc )
		return 0;

	return (void*) result.start;
}

static void
Free_VMM_Page(
	void *			page
) 
{
	struct pmem_region	query;
	struct pmem_region	result;

	pmem_region_unset_all(&query);

	query.start	= (uintptr_t)( page );
	query.end	= (uintptr_t)( page + PAGE_SIZE );

	int rc = pmem_query(&query,&result);

	if( rc )
	{
		panic( "Asked to free non-allocated page %p! rc=%d",
			page,
			rc
		);
		return;
	}

	result.allocated = 0;
	pmem_update(&result);
}



void
send_key_to_vmm(
	unsigned char		status,
	unsigned char		scancode
)
{
	if( !g_vm_guest )
		return;

	struct v3_keyboard_event evt = {
		.status		= status,
		.scan_code	= scancode,
	};

	v3_deliver_keyboard_event( g_vm_guest, &evt );
}


void
send_mouse_to_vmm(
	unsigned char		packet[3]
)
{
	if( !g_vm_guest )
		return;

	struct v3_mouse_event evt;
	memcpy(evt.data, packet, 3);

	v3_deliver_mouse_event(g_vm_guest, &evt);
}


void
send_tick_to_vmm(
	unsigned int		period_us
)
{
	if( !g_vm_guest )
		return;

	struct v3_timer_event evt = {
		.period_us	= period_us,
	};

	v3_deliver_timer_event( g_vm_guest, &evt );
}


void
translate_intr_handler(
	struct pt_regs *	regs,
	unsigned int		vector
) 
{
	struct v3_interrupt intr = {
		.irq		= vector-32,
		.error		= regs->orig_rax,
		.should_ack	= 0,
	};

	//  PrintBoth("translate_intr_handler: opaque=0x%x\n",mystate.opaque);
	printk( "%s: irq %d\n", __func__, vector );
	v3_deliver_irq( irq_to_guest_map[intr.irq], &intr );
}


int
kitten_hook_interrupt(
	struct guest_info *	vm,
	unsigned int		irq
)
{
	if( irq_to_guest_map[irq] ) {
		//PrintBoth("Attempt to hook interrupt that is already hooked\n");
		return -1;
	}

	//PrintBoth("Hooked interrupt 0x%x with opaque 0x%x\n", irq, vm);
	printk( "%s: hook irq %d to %p\n", __func__, irq, vm );
	irq_to_guest_map[irq] = vm;

	set_idtvec_handler( irq, translate_intr_handler );
	return 0;
}


static int
ack_irq(
	int			irq
) 
{
	lapic_ack_interrupt();
	return 0;
}

  
static unsigned int
get_cpu_khz( void ) 
{
	return cpu_info[0].arch.cur_cpu_khz;
}

static void *
v3vee_alloc(
	unsigned int size
)
{
	return kmem_alloc( size );
}


static void
v3vee_free(
	void * addr
)
{
	return kmem_free( addr );
}


static void
v3vee_printk(
	const char *		fmt,
	...
)
{
	va_list ap;
	va_start( ap, fmt );
	vprintk( fmt, ap );
	va_end( ap );
}


struct v3_os_hooks v3vee_os_hooks = {
	.print_debug		= v3vee_printk,  // serial print ideally
	.print_info		= v3vee_printk,   // serial print ideally
	.print_trace		= v3vee_printk,  // serial print ideally
	.allocate_pages		= Allocate_VMM_Pages, // defined in vmm_stubs
	.free_page		= Free_VMM_Page, // defined in vmm_stubs
	.malloc			= v3vee_alloc,
	.free			= v3vee_free,
	.vaddr_to_paddr		= kitten_va_to_pa,
	.paddr_to_vaddr		= kitten_pa_to_va,
	.hook_interrupt		= kitten_hook_interrupt,
	.ack_irq		= ack_irq,
	.get_cpu_khz		= get_cpu_khz,
};


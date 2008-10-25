/* Copyright (c) 2007,2008 Sandia National Laboratories */

#ifndef _LWK_PALACIOS_H_
#define _LWK_PALACIOS_H_

#ifdef CONFIG_V3VEE

#include <lwk/types.h>
#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>

extern struct guest_info * g_vm_guest;


extern void
v3vee_init_stubs( void );


extern int
v3vee_run_vmm( void );


extern struct v3_os_hooks v3vee_os_hooks;

/**** 
 * 
 * stubs called by geekos....
 * 
 ***/
void send_key_to_vmm(unsigned char status, unsigned char scancode);
void send_mouse_to_vmm(unsigned char packet[3]);
void send_tick_to_vmm(unsigned int period_us);


/* Location of the ROM Bios and VGA Bios used by palacios */
extern uint8_t rombios_start, rombios_end;
extern uint8_t vgabios_start, vgabios_end;
extern paddr_t initrd_start, initrd_end;


#endif // CONFIG_V3VEE

#endif

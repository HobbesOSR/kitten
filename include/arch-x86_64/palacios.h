#ifndef _PALACIOS_H
#define _PALACIOS_H

#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>

/**
 * These are used by the kernel to forward events to Palacios.
 */
extern void send_key_to_palacios(unsigned char status, unsigned char scan_code);
extern void send_mouse_to_palacios(unsigned char packet[3]);
extern void send_tick_to_palacios(unsigned int period_us);

/**
 * Starts a guest OS running...
 */
extern int palacios_run_guest(void);

#endif

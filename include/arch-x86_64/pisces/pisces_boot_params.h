/*
 * Pisces Booting Protocol
 * This file is shared with enclave OS
 */
#ifndef _PISCES_BOOT_PARAMS_H_
#define _PISCES_BOOT_PARAMS_H_

#include <arch/pisces/pisces_pgtables.h>


#define PISCES_MAGIC 0x000FE110

struct pisces_enclave;


/* Pisces Boot loader memory layout
 * 1. boot parameters             // 4KB aligned
 *     ->  Trampoline code sits at the start of this structure 
 * 2. Console ring buffer (64KB)  // 4KB aligned
 * 3. To enclave CMD buffer       // (4KB)
 * 4. From enclave CMD buffer     // (4KB)
 * 5. kernel image                // 2M aligned
 * 6. initrd                      // 2M aligned
 *
 */



#define LAUNCH_CODE_SIZE      64
#define LAUNCH_CODE_DATA_RSI  6
#define LAUNCH_CODE_DATA_RIP  7

/* All addresses in this structure are physical addresses */
struct pisces_boot_params {

	/* Embedded asm to load esi and jump to kernel */
	u64 launch_code[8]; 

	u8 init_dbg_buf[16];


	u64 magic;

	u64 boot_params_size;

	u64 cpu_id;
	u64 apic_id;
	u64 cpu_khz;

	u64 trampoline_code_pa;

	/* coordinator domain cpu apic id */
	u64 domain_xcall_master_apicid;

	/* domain cross call vector id */
	u64 domain_xcall_vector;

	/* cmd_line */
	char cmd_line[1024];

	/* kernel */
	u64 kernel_addr;
	u64 kernel_size;

	/* initrd */
	u64 initrd_addr;
	u64 initrd_size;
	

	/* The address of the ring buffer used for the early console */
	u64 console_ring_addr;
	u64 console_ring_size;

	/* Address and size of the linux->enclave command/control channel */
	u64 control_buf_addr;
	u64 control_buf_size;

	/* Address and size of the enclave->linux command/control channel */
	u64 longcall_buf_addr;
	u64 longcall_buf_size;

	/* Address and size of the enclave->linux XPMEM channel */
	u64 xpmem_buf_addr;
	u64 xpmem_buf_size;

	u64 base_mem_paddr;
	u64 base_mem_size;


} __attribute__((packed));

#endif

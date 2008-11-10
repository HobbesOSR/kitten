/** \file
 * i386 GDT/IDT/LDT entry structures.
 *
 * Written 2000 by Andi Kleen
 */ 
#ifndef _ARCH_DESC_H
#define _ARCH_DESC_H

#ifndef __ASSEMBLY__

#include <lwk/string.h>
#include <lwk/smp.h>

#include <arch/segment.h>
#include <arch/mmu.h>

/** GDT entries.
 * 8 byte segment descriptor
 */
struct desc_struct { 
	u16 limit0;
	u16 base0;
	unsigned base1 : 8, type : 4, s : 1, dpl : 2, p : 1;
	unsigned limit : 4, avl : 1, l : 1, d : 1, g : 1, base2 : 8;
} __attribute__((packed)); 

sizecheck_struct( desc_struct, 8 );

extern struct desc_struct cpu_gdt_table[GDT_ENTRIES];

enum { 
	GATE_INTERRUPT = 0xE, 
	GATE_TRAP = 0xF, 	
	GATE_CALL = 0xC,
}; 	

/**
 * Long-Mode Gate Descriptor (16-bytes)
 */
struct gate_struct {          
	uint16_t offset_low;	/**< [15-0] of target code segment offset */
	uint16_t segment; 	/**< Target code segment selector */
	unsigned ist    : 3;	/**< Interrupt-Stack-Table index into TSS */
	unsigned zero0  : 5;	/**< Must be zero */
	unsigned type   : 5;	/**< Gate descriptor type */
	unsigned dpl    : 2;	/**< Privilege level */
	unsigned p      : 1;	/**< Present bit... in use? */
	uint16_t offset_middle;	/**< [31-24] of target code segment offset */
	uint32_t offset_high;	/**< [63-32] of target code segment offset */
	uint32_t zero1;		/**< Must be zero */
} __attribute__((packed));

sizecheck_struct( gate_struct, 16 );

/** \name Pointer spliting.
 * The bits of the pointer in the descriptors are split into
 * low, middle and high portions since the structure was not
 * designed to be 64-bit clean when it was first laid out in 1980.
 *
 * @{
 */
#define PTR_LOW(x) ((unsigned long)(x) & 0xFFFF) 
#define PTR_MIDDLE(x) (((unsigned long)(x) >> 16) & 0xFFFF)
#define PTR_HIGH(x) ((unsigned long)(x) >> 32)
// @}

/** Gate descriptor types */
enum { 
	DESC_TSS = 0x9,
	DESC_LDT = 0x2,
}; 

/** LDT or TSS descriptor in the GDT.
 * 16 bytes.
 */
struct ldttss_desc { 
	u16 limit0;
	u16 base0;
	unsigned base1 : 8, type : 5, dpl : 2, p : 1;
	unsigned limit1 : 4, zero0 : 3, g : 1, base2 : 8;
	u32 base3;
	u32 zero1; 
} __attribute__((packed)); 

struct desc_ptr {
	unsigned short size;
	unsigned long address;
} __attribute__((packed)) ;

extern struct desc_ptr idt_descr;


#define load_TR_desc() asm volatile("ltr %w0"::"r" (GDT_ENTRY_TSS*8))
#define load_LDT_desc() asm volatile("lldt %w0"::"r" (GDT_ENTRY_LDT*8))
#define clear_LDT()  asm volatile("lldt %w0"::"r" (0))

/**
 * This is the ldt that every process will get unless we need
 * something other than this.
 */
extern struct desc_struct default_ldt[];
extern struct gate_struct idt_table[]; 
extern struct desc_ptr cpu_gdt_descr[];

/** Retrieve the GDT for a given cpu */
#define cpu_gdt(_cpu) ((struct desc_struct *)cpu_gdt_descr[_cpu].address)

/**
 * Installs a Long-Mode gate descriptor.
 */
static inline void
_set_gate(
	void *        addr,	/**< Address to install gate descriptor at */
	unsigned      type,	/**< Type of gate */
	unsigned long func,	/**< The handler function for the gate */
	unsigned      dpl,	/**< Privilege level */
	unsigned      ist	/**< Interupt-Stack-Table index */
)  
{
	struct gate_struct s = {
		.offset_low	= PTR_LOW(func),
		.segment	= __KERNEL_CS,
		.ist		= ist,
		.p		= 1,
		.dpl		= dpl,
		.zero0		= 0,
		.zero1		= 0,
		.type		= type,
		.offset_middle	= PTR_MIDDLE(func),
		.offset_high	= PTR_HIGH(func),
	};

	/* does not need to be atomic because it is only at setup time */ 
	*(struct gate_struct *) addr = s;
} 


/**
 * Installs an interrupt gate in the IDT.
 *
 * The interrupt will execute on the normal kernel stack with
 * a hardware interrupt stack frame if ist is NULL, otherwise it
 * will run on the specified stack.
 *
 * \note This is only used by the low-level interrupts_init() call
 * to install the asm_handler() function.  To install a kernel interrupt
 * handler use set_idtvec_handler() instead.
 */
static inline void
set_intr_gate_ist(
	int			nr,
	void *			func,
	unsigned		ist	//!< If 0 run on the kernel stack
) 
{ 
	BUG_ON((unsigned)nr > 0xFF);

	_set_gate(
		&idt_table[nr],
		GATE_INTERRUPT,
		(unsigned long) func,
		0,
		ist
	); 
} 

/** Set the interrupt gate to run on the kernel stack */
#define set_intr_gate( nr, func ) set_intr_gate_ist( nr, func, 0 )


/** Build a TSS descriptor for the LDT */
static inline void
set_tssldt_descriptor(
	void *			ptr,	//!< Pointer into the LDT
	unsigned long		tss,
	unsigned		type, 
	unsigned		size
)
{ 
	struct ldttss_desc d = {
		.limit0		= size & 0xFFFF,
		.base0		= PTR_LOW(tss),
		.base1		= PTR_MIDDLE(tss) & 0xFF,
		.type		= type,
		.p		= 1,
		.limit1		= (size >> 16) & 0xF,
		.base2		= (PTR_MIDDLE(tss) >> 8) & 0xFF,
		.base3		= PTR_HIGH(tss),
	};

	*(struct ldttss_desc*) ptr = d;
}


/**
 * Set the TSS in the GDT for the given CPU.
 */
static inline void
set_tss_desc(
	unsigned		cpu,
	void *			addr
)
{ 
	/*
	 * sizeof(unsigned long) coming from an extra "long" at the end
	 * of the iobitmap. See tss_struct definition in processor.h
	 *
	 * -1? seg base+limit should be pointing to the address of the
	 * last valid byte
	 */
	set_tssldt_descriptor(
		&cpu_gdt(cpu)[GDT_ENTRY_TSS],
		(unsigned long)addr,
		DESC_TSS,
		IO_BITMAP_OFFSET + IO_BITMAP_BYTES + sizeof(unsigned long) - 1
	);
} 


/**
 * Set the LDT in the GDT for the given CPU.
 */
static inline void
set_ldt_desc(
	unsigned		cpu,
	void *			addr,
	int			size
)
{ 
	set_tssldt_descriptor(
		&cpu_gdt(cpu)[GDT_ENTRY_LDT],
		(unsigned long)addr,
		DESC_LDT,
		size * 8 - 1
	);
}



#endif /* !__ASSEMBLY__ */

#endif

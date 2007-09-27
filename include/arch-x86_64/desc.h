/* Written 2000 by Andi Kleen */ 
#ifndef _ARCH_DESC_H
#define _ARCH_DESC_H

#include <arch/ldt.h>

#ifndef __ASSEMBLY__

#include <lwk/string.h>
#include <lwk/smp.h>

#include <arch/segment.h>
#include <arch/mmu.h>

// 8 byte segment descriptor
struct desc_struct { 
	u16 limit0;
	u16 base0;
	unsigned base1 : 8, type : 4, s : 1, dpl : 2, p : 1;
	unsigned limit : 4, avl : 1, l : 1, d : 1, g : 1, base2 : 8;
} __attribute__((packed)); 

struct n_desc_struct { 
	unsigned int a,b;
}; 	

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
	uint16_t offset_low;	/* [15-0] of target code segment offset */
	uint16_t segment; 	/* Target code segment selector */
	unsigned ist    : 3;	/* Interrupt-Stack-Table index into TSS */
	unsigned zero0  : 5;
	unsigned type   : 5;	/* Gate descriptor type */
	unsigned dpl    : 2;	/* Privilege level */
	unsigned p      : 1;	/* Present bit... in use? */
	uint16_t offset_middle;	/* [31-24] of target code segment offset */
	uint32_t offset_high;	/* [63-32] of target code segment offset */
	uint32_t zero1;
} __attribute__((packed));

#define PTR_LOW(x) ((unsigned long)(x) & 0xFFFF) 
#define PTR_MIDDLE(x) (((unsigned long)(x) >> 16) & 0xFFFF)
#define PTR_HIGH(x) ((unsigned long)(x) >> 32)

enum { 
	DESC_TSS = 0x9,
	DESC_LDT = 0x2,
}; 

// LDT or TSS descriptor in the GDT. 16 bytes.
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

#define load_TR_desc() asm volatile("ltr %w0"::"r" (GDT_ENTRY_TSS*8))
#define load_LDT_desc() asm volatile("lldt %w0"::"r" (GDT_ENTRY_LDT*8))
#define clear_LDT()  asm volatile("lldt %w0"::"r" (0))

/*
 * This is the ldt that every process will get unless we need
 * something other than this.
 */
extern struct desc_struct default_ldt[];
extern struct gate_struct idt_table[]; 
extern struct desc_ptr cpu_gdt_descr[];

/* the cpu gdt accessor */
#define cpu_gdt(_cpu) ((struct desc_struct *)cpu_gdt_descr[_cpu].address)

/**
 * Installs a Long-Mode gate descriptor.
 */
static inline void
_set_gate(
	void *        adr,	/* Address to install gate descriptor at */
	unsigned      type,	/* Type of gate */
	unsigned long func,	/* The handler function for the gate */
	unsigned      dpl,	/* Privilege level */
	unsigned      ist	/* Interupt-Stack-Table index */
)  
{
	struct gate_struct s; 	
	s.offset_low = PTR_LOW(func); 
	s.segment = __KERNEL_CS;
	s.ist = ist; 
	s.p = 1;
	s.dpl = dpl; 
	s.zero0 = 0;
	s.zero1 = 0; 
	s.type = type; 
	s.offset_middle = PTR_MIDDLE(func); 
	s.offset_high = PTR_HIGH(func); 
	/* does not need to be atomic because it is only done once at setup time */ 
	memcpy(adr, &s, 16); 
} 

/**
 * Installs an interrupt gate.
 * The interrupt will execute on the normal kernel stack.
 */
static inline void
set_intr_gate(int nr, void *func) 
{ 
	BUG_ON((unsigned)nr > 0xFF);
	_set_gate(&idt_table[nr], GATE_INTERRUPT, (unsigned long) func, 0, 0); 
} 

/**
 * Installs an interrupt gate.
 * The interrupt will execute on the stack specified by the 'ist' argument.
 */
static inline void set_intr_gate_ist(int nr, void *func, unsigned ist) 
{ 
	BUG_ON((unsigned)nr > 0xFF);
	_set_gate(&idt_table[nr], GATE_INTERRUPT, (unsigned long) func, 0, ist); 
} 

/**
 * Installs a system interrupt gate.
 * The privilege level is set to 3, meaning that user-mode can trigger it.
 */
static inline void set_system_gate(int nr, void *func) 
{ 
	BUG_ON((unsigned)nr > 0xFF);
	_set_gate(&idt_table[nr], GATE_INTERRUPT, (unsigned long) func, 3, 0); 
} 

/**
 * Installs a system interrupt gate.
 * The privilege level is set to 3, meaning that user-mode can trigger it.
 * The interrupt will execute on the stack specified by the 'ist' argument.
 */
static inline void set_system_gate_ist(int nr, void *func, unsigned ist)
{
	_set_gate(&idt_table[nr], GATE_INTERRUPT, (unsigned long) func, 3, ist);
}

static inline void set_tssldt_descriptor(void *ptr, unsigned long tss, unsigned type, 
					 unsigned size) 
{ 
	struct ldttss_desc d;
	memset(&d,0,sizeof(d)); 
	d.limit0 = size & 0xFFFF;
	d.base0 = PTR_LOW(tss); 
	d.base1 = PTR_MIDDLE(tss) & 0xFF; 
	d.type = type;
	d.p = 1; 
	d.limit1 = (size >> 16) & 0xF;
	d.base2 = (PTR_MIDDLE(tss) >> 8) & 0xFF; 
	d.base3 = PTR_HIGH(tss); 
	memcpy(ptr, &d, 16); 
}

static inline void set_tss_desc(unsigned cpu, void *addr)
{ 
	/*
	 * sizeof(unsigned long) coming from an extra "long" at the end
	 * of the iobitmap. See tss_struct definition in processor.h
	 *
	 * -1? seg base+limit should be pointing to the address of the
	 * last valid byte
	 */
	set_tssldt_descriptor(&cpu_gdt(cpu)[GDT_ENTRY_TSS],
		(unsigned long)addr, DESC_TSS,
		IO_BITMAP_OFFSET + IO_BITMAP_BYTES + sizeof(unsigned long) - 1);
} 

static inline void set_ldt_desc(unsigned cpu, void *addr, int size)
{ 
	set_tssldt_descriptor(&cpu_gdt(cpu)[GDT_ENTRY_LDT], (unsigned long)addr,
			      DESC_LDT, size * 8 - 1);
}

static inline void set_seg_base(unsigned cpu, int entry, void *base)
{ 
	struct desc_struct *d = &cpu_gdt(cpu)[entry];
	u32 addr = (u32)(u64)base;
	BUG_ON((u64)base >> 32); 
	d->base0 = addr & 0xffff;
	d->base1 = (addr >> 16) & 0xff;
	d->base2 = (addr >> 24) & 0xff;
} 

#define LDT_entry_a(info) \
	((((info)->base_addr & 0x0000ffff) << 16) | ((info)->limit & 0x0ffff))
/* Don't allow setting of the lm bit. It is useless anyways because 
   64bit system calls require __USER_CS. */ 
#define LDT_entry_b(info) \
	(((info)->base_addr & 0xff000000) | \
	(((info)->base_addr & 0x00ff0000) >> 16) | \
	((info)->limit & 0xf0000) | \
	(((info)->read_exec_only ^ 1) << 9) | \
	((info)->contents << 10) | \
	(((info)->seg_not_present ^ 1) << 15) | \
	((info)->seg_32bit << 22) | \
	((info)->limit_in_pages << 23) | \
	((info)->useable << 20) | \
	/* ((info)->lm << 21) | */ \
	0x7000)

#define LDT_empty(info) (\
	(info)->base_addr	== 0	&& \
	(info)->limit		== 0	&& \
	(info)->contents	== 0	&& \
	(info)->read_exec_only	== 1	&& \
	(info)->seg_32bit	== 0	&& \
	(info)->limit_in_pages	== 0	&& \
	(info)->seg_not_present	== 1	&& \
	(info)->useable		== 0	&& \
	(info)->lm		== 0)

#if TLS_SIZE != 24
# error update this code.
#endif

static inline void load_TLS(struct thread_struct *t, unsigned int cpu)
{
	u64 *gdt = (u64 *)(cpu_gdt(cpu) + GDT_ENTRY_TLS_MIN);
	gdt[0] = t->tls_array[0];
	gdt[1] = t->tls_array[1];
	gdt[2] = t->tls_array[2];
} 

/*
 * load one particular LDT into the current CPU
 */
static inline void load_LDT_nolock (mm_context_t *pc, int cpu)
{
	int count = pc->size;

	if (likely(!count)) {
		clear_LDT();
		return;
	}
		
	set_ldt_desc(cpu, pc->ldt, count);
	load_LDT_desc();
}

static inline void load_LDT(mm_context_t *pc)
{
	int cpu = get_cpu();
	load_LDT_nolock(pc, cpu);
	put_cpu();
}

extern struct desc_ptr idt_descr;

#endif /* !__ASSEMBLY__ */

#endif

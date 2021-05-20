/*
 * ld script to make ARM Linux kernel
 * taken from the i386 version by Russell King
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include <arch-generic/vmlwk.lds.h>

/* JRL: Hopefully these aren't needed...
#include <asm/thread_info.h>
#include <asm/memory.h>
*/

#include <arch/page.h>
#include <lwk/sizes.h>

#define ARM_EXIT_KEEP(x)
#define ARM_EXIT_DISCARD(x)	x

OUTPUT_ARCH(aarch64)
ENTRY(stext)

/*jiffies = jiffies_64;*/


#if 0
#define HYPERVISOR_TEXT					\
	/*						\
	 * Force the alignment to be compatible with	\
	 * the vectors requirements			\
	 */						\
	. = ALIGN(2048);				\
	VMLINUX_SYMBOL(__hyp_idmap_text_start) = .;	\
	*(.hyp.idmap.text)				\
	VMLINUX_SYMBOL(__hyp_idmap_text_end) = .;	\
	VMLINUX_SYMBOL(__hyp_text_start) = .;		\
	*(.hyp.text)					\
	VMLINUX_SYMBOL(__hyp_text_end) = .;

#endif

PHDRS {
 text PT_LOAD FLAGS(5); /* R_E */
 data PT_LOAD FLAGS(7); /* RWE */
 user PT_LOAD FLAGS(7); /* RWE */
 data.init PT_LOAD FLAGS(7); /* RWE */
 note PT_NOTE FLAGS(4); /* R__ */
}

SECTIONS
{
	/*
	 * XXX: The linker does not define how output sections are
	 * assigned to input sections when there are multiple statements
	 * matching the same input section name.  There is no documented
	 * order of matching.
	 */
	/*
    /DISCARD/ : {
		ARM_EXIT_DISCARD(EXIT_TEXT)
		ARM_EXIT_DISCARD(EXIT_DATA)
		EXIT_CALL
		*(.discard)
		*(.discard.*)
	}
    */

	. = PAGE_OFFSET + TEXT_OFFSET;

/* 
    .head.text : {

		HEAD_TEXT
	}
    */
	_text = .;
	.text : AT(ADDR(.text) - LOAD_OFFSET) {			/* Real text segment		*/
            *(.bootstrap.text)
		_stext = .;		/* Text and read-only data	*/
			__exception_text_start = .;
			*(.exception.text)
			__exception_text_end = .;
/*			IRQENTRY_TEXT*/
			TEXT_TEXT
			SCHED_TEXT
			LOCK_TEXT
/*			HYPERVISOR_TEXT */ 
			*(.fixup)
			*(.gnu.warning)
		. = ALIGN(16);
		*(.got)			/* Global offset table		*/
	}

	RO_DATA(PAGE_SIZE)
/*	NOTES */
	_etext = .;			/* End of text and rodata section */


    /* 	EXCEPTION_TABLE(8)*/
    . = ALIGN(16);		/* Exception table */
    __start___ex_table = .;
    __ex_table : AT(ADDR(__ex_table) - LOAD_OFFSET) { *(__ex_table) }
    __stop___ex_table = .;


	__init_end = .;


	. = ALIGN(PAGE_SIZE);
	_data = .;
	__data_loc = _data;
	_sdata = .;

/*  . = ALIGN(PAGE_SIZE); */        /* Align data segment to page size boundary */
				/* Data */
  .data : AT(ADDR(.data) - LOAD_OFFSET) {
	DATA_DATA
	CONSTRUCTORS
	} :data

  _edata = .;			/* End of data section */

  . = ALIGN(PAGE_SIZE);
  . = ALIGN(CONFIG_ARM64_L1_CACHE_BYTES);
  .data.cacheline_aligned : AT(ADDR(.data.cacheline_aligned) - LOAD_OFFSET) {
	*(.data.cacheline_aligned)
  }
  . = ALIGN(CONFIG_ARM64_INTERNODE_CACHE_BYTES);
  .data.read_mostly : AT(ADDR(.data.read_mostly) - LOAD_OFFSET) {
  	*(.data.read_mostly)
  }


 . = ALIGN(8192);		/* bootstrap_task */
  .data.bootstrap_task : AT(ADDR(.data.bootstrap_task) - LOAD_OFFSET) {
	*(.data.bootstrap_task)
  }:data.init

  . = ALIGN(4096);
  .data.page_aligned : AT(ADDR(.data.page_aligned) - LOAD_OFFSET) {
	*(.data.page_aligned)
  }

	. = ALIGN(PAGE_SIZE);
	__init_begin = .;

    /* 	INIT_TEXT_SECTION(8)*/
    . = ALIGN(4096);		/* Init code and data */
    __init_begin = .;
    .init.text : AT(ADDR(.init.text) - LOAD_OFFSET) {
	    _sinittext = .;
	    *(.init.text)
	    _einittext = .;
    }


__initdata_begin = .;
  .init.data : AT(ADDR(.init.data) - LOAD_OFFSET) { *(.init.data) }
  __initdata_end = .;
  . = ALIGN(16);
  __setup_start = .;
  .init.setup : AT(ADDR(.init.setup) - LOAD_OFFSET) { *(.init.setup) }
  __setup_end = .;
  __initcall_start = .;
  .initcall.init : AT(ADDR(.initcall.init) - LOAD_OFFSET) {
	INITCALLS
  }
  __initcall_end = .;
  __con_initcall_start = .;
  .con_initcall.init : AT(ADDR(.con_initcall.init) - LOAD_OFFSET) {
	*(.con_initcall.init)
  }
  __con_initcall_end = .;
  SECURITY_INIT
  . = ALIGN(8);
  __alt_instructions = .;
  .altinstructions : AT(ADDR(.altinstructions) - LOAD_OFFSET) {
	*(.altinstructions)
  }
  __alt_instructions_end = .; 
  .altinstr_replacement : AT(ADDR(.altinstr_replacement) - LOAD_OFFSET) {
	*(.altinstr_replacement)
  }

  /* .exit.text is discard at runtime, not link time, to deal with references
     from .altinstructions and .eh_frame */
  .exit.text : AT(ADDR(.exit.text) - LOAD_OFFSET) { *(.exit.text) }
  .exit.data : AT(ADDR(.exit.data) - LOAD_OFFSET) { *(.exit.data) }


#ifdef CONFIG_BLK_DEV_INITRD
  . = ALIGN(4096);
  __initramfs_start = .;
  .init.ramfs : AT(ADDR(.init.ramfs) - LOAD_OFFSET) { *(.init.ramfs) }
  __initramfs_end = .;
#endif

/* 	.exit.text : {
		ARM_EXIT_KEEP(EXIT_TEXT)
	}
	. = ALIGN(16);
	.init.data : {
		INIT_DATA
		INIT_SETUP(16)
		INIT_CALLS
		CON_INITCALL
		SECURITY_INITCALL
		INIT_RAM_FS
	}
	.exit.data : {
		ARM_EXIT_KEEP(EXIT_DATA)
	}
*/


	/*
	 * Data written with the MMU off but read with the MMU on requires
	 * cache lines to be invalidated, discarding up to a Cache Writeback
	 * Granule (CWG) of data from the cache. Keep the section that
	 * requires this type of maintenance to be in its own Cache Writeback
	 * Granule (CWG) area so the cache maintenance operations don't
	 * interfere with adjacent data.
	 */
	.mmuoff.data.write : ALIGN(SZ_2K) {
		__mmuoff_data_start = .;
		*(.mmuoff.data.write)
	}
	. = ALIGN(SZ_2K);
	.mmuoff.data.read : {
		*(.mmuoff.data.read)
		__mmuoff_data_end = .;
	}


  . = ALIGN(4096);
  __per_cpu_start = .;
  .data.percpu  : AT(ADDR(.data.percpu) - LOAD_OFFSET) { *(.data.percpu) }
  __per_cpu_end = .;
  . = ALIGN(4096);
  __init_end = .;

  . = ALIGN(4096);
  __nosave_begin = .;
  .data_nosave : AT(ADDR(.data_nosave) - LOAD_OFFSET) { *(.data.nosave) }
  . = ALIGN(4096);
  __nosave_end = .;


/*	BSS_SECTION(0, 0, 0)*/
  __bss_start = .;		/* BSS */
  .bss : AT(ADDR(.bss) - LOAD_OFFSET) {
	*(.bss.page_aligned)
	*(.bss)
	}
  __bss_stop = .;

	_end = .;

/* 	STABS_DEBUG*/
}

/*
 * The HYP init code can't be more than a page long.
 */
/*ASSERT(((__hyp_idmap_text_start + PAGE_SIZE) > __hyp_idmap_text_end),
       "HYP init code too big")
       */

#include <arch-generic/vmlwk.lds.h>
#include <arch/page.h>

OUTPUT_FORMAT(CONFIG_OUTPUT_FORMAT, CONFIG_OUTPUT_FORMAT, CONFIG_OUTPUT_FORMAT)

#undef i386

#if defined(CONFIG_MK1OM)
OUTPUT_ARCH(k1om)
#else
OUTPUT_ARCH(i386:x86-64)
#endif

ENTRY(startup_64)

SECTIONS
{
	/* Be careful parts of head.S assume startup_32 is at
	 * address 0.
	 */
	. = 0;
	.text :	{
		_head = . ;
		*(.text.head)
		_ehead = . ;
		*(.text.compressed)
		_text = .; 	/* Text */
		*(.text)
		*(.text.*)
		_etext = . ;
	}
	.rodata : {
		_rodata = . ;
		*(.rodata)	 /* read-only data */
		*(.rodata.*)
		_erodata = . ;
	}
	.data :	{
		_data = . ;
		*(.data)
		*(.data.*)
		_edata = . ;
	}
	. = ALIGN(CONFIG_X86_L1_CACHE_BYTES);
	.bss : {
		_bss = . ;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(8);	/* For convenience during zeroing */
		_end = . ;
		. = ALIGN(4096);
		pgtable = . ;
		. = . + 4096 * 6;
		_heap = . ;
	}
}

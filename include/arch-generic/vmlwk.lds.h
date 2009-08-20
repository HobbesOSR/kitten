#ifndef LOAD_OFFSET
#define LOAD_OFFSET 0
#endif

#ifndef VMLWK_SYMBOL
#define VMLWK_SYMBOL(_sym_) _sym_
#endif

/* Align . to a 8 byte boundary equals to maximum function alignment. */
#define ALIGN_FUNCTION()  . = ALIGN(8)

/* .data section */
#define DATA_DATA							\
	*(.data)							\
	*(.data.init.refok)

#define RO_DATA(align)							\
	. = ALIGN((align));						\
	.rodata           : AT(ADDR(.rodata) - LOAD_OFFSET) {		\
		VMLWK_SYMBOL(__start_rodata) = .;			\
		*(.rodata) *(.rodata.*)					\
		*(__vermagic)		/* Kernel version magic */	\
	}								\
									\
	.rodata1          : AT(ADDR(.rodata1) - LOAD_OFFSET) {		\
		*(.rodata1)						\
	}								\
									\
	/* PCI quirks */						\
	.pci_fixup        : AT(ADDR(.pci_fixup) - LOAD_OFFSET) {	\
		VMLWK_SYMBOL(__start_pci_fixups_early) = .;		\
		*(.pci_fixup_early)					\
		VMLWK_SYMBOL(__end_pci_fixups_early) = .;		\
		VMLWK_SYMBOL(__start_pci_fixups_header) = .;		\
		*(.pci_fixup_header)					\
		VMLWK_SYMBOL(__end_pci_fixups_header) = .;		\
		VMLWK_SYMBOL(__start_pci_fixups_final) = .;		\
		*(.pci_fixup_final)					\
		VMLWK_SYMBOL(__end_pci_fixups_final) = .;		\
		VMLWK_SYMBOL(__start_pci_fixups_enable) = .;		\
		*(.pci_fixup_enable)					\
		VMLWK_SYMBOL(__end_pci_fixups_enable) = .;		\
		VMLWK_SYMBOL(__start_pci_fixups_resume) = .;		\
		*(.pci_fixup_resume)					\
		VMLWK_SYMBOL(__end_pci_fixups_resume) = .;		\
		VMLWK_SYMBOL(__start_pci_fixups_resume_early) = .;	\
		*(.pci_fixup_resume_early)				\
		VMLWK_SYMBOL(__end_pci_fixups_resume_early) = .;	\
		VMLWK_SYMBOL(__start_pci_fixups_suspend) = .;		\
		*(.pci_fixup_suspend)					\
		VMLWK_SYMBOL(__end_pci_fixups_suspend) = .;		\
	}								\
									\
	/* RapidIO route ops */						\
	.rio_route        : AT(ADDR(.rio_route) - LOAD_OFFSET) {	\
		VMLWK_SYMBOL(__start_rio_route_ops) = .;		\
		*(.rio_route_ops)					\
		VMLWK_SYMBOL(__end_rio_route_ops) = .;			\
	}								\
									\
	/* Kernel symbol table: Normal symbols */			\
	__ksymtab         : AT(ADDR(__ksymtab) - LOAD_OFFSET) {		\
		VMLWK_SYMBOL(__start___ksymtab) = .;			\
		*(__ksymtab)						\
		VMLWK_SYMBOL(__stop___ksymtab) = .;			\
	}								\
									\
	/* Kernel symbol table: GPL-only symbols */			\
	__ksymtab_gpl     : AT(ADDR(__ksymtab_gpl) - LOAD_OFFSET) {	\
		VMLWK_SYMBOL(__start___ksymtab_gpl) = .;		\
		*(__ksymtab_gpl)					\
		VMLWK_SYMBOL(__stop___ksymtab_gpl) = .;			\
	}								\
									\
	/* Kernel symbol table: Normal unused symbols */		\
	__ksymtab_unused  : AT(ADDR(__ksymtab_unused) - LOAD_OFFSET) {	\
		VMLWK_SYMBOL(__start___ksymtab_unused) = .;		\
		*(__ksymtab_unused)					\
		VMLWK_SYMBOL(__stop___ksymtab_unused) = .;		\
	}								\
									\
	/* Kernel symbol table: GPL-only unused symbols */		\
	__ksymtab_unused_gpl : AT(ADDR(__ksymtab_unused_gpl) - LOAD_OFFSET) { \
		VMLWK_SYMBOL(__start___ksymtab_unused_gpl) = .;		\
		*(__ksymtab_unused_gpl)					\
		VMLWK_SYMBOL(__stop___ksymtab_unused_gpl) = .;		\
	}								\
									\
	/* Kernel symbol table: GPL-future-only symbols */		\
	__ksymtab_gpl_future : AT(ADDR(__ksymtab_gpl_future) - LOAD_OFFSET) { \
		VMLWK_SYMBOL(__start___ksymtab_gpl_future) = .;		\
		*(__ksymtab_gpl_future)					\
		VMLWK_SYMBOL(__stop___ksymtab_gpl_future) = .;		\
	}								\
									\
	/* Kernel symbol table: Normal symbols */			\
	__kcrctab         : AT(ADDR(__kcrctab) - LOAD_OFFSET) {		\
		VMLWK_SYMBOL(__start___kcrctab) = .;			\
		*(__kcrctab)						\
		VMLWK_SYMBOL(__stop___kcrctab) = .;			\
	}								\
									\
	/* Kernel symbol table: GPL-only symbols */			\
	__kcrctab_gpl     : AT(ADDR(__kcrctab_gpl) - LOAD_OFFSET) {	\
		VMLWK_SYMBOL(__start___kcrctab_gpl) = .;		\
		*(__kcrctab_gpl)					\
		VMLWK_SYMBOL(__stop___kcrctab_gpl) = .;			\
	}								\
									\
	/* Kernel symbol table: Normal unused symbols */		\
	__kcrctab_unused  : AT(ADDR(__kcrctab_unused) - LOAD_OFFSET) {	\
		VMLWK_SYMBOL(__start___kcrctab_unused) = .;		\
		*(__kcrctab_unused)					\
		VMLWK_SYMBOL(__stop___kcrctab_unused) = .;		\
	}								\
									\
	/* Kernel symbol table: GPL-only unused symbols */		\
	__kcrctab_unused_gpl : AT(ADDR(__kcrctab_unused_gpl) - LOAD_OFFSET) { \
		VMLWK_SYMBOL(__start___kcrctab_unused_gpl) = .;		\
		*(__kcrctab_unused_gpl)					\
		VMLWK_SYMBOL(__stop___kcrctab_unused_gpl) = .;		\
	}								\
									\
	/* Kernel symbol table: GPL-future-only symbols */		\
	__kcrctab_gpl_future : AT(ADDR(__kcrctab_gpl_future) - LOAD_OFFSET) { \
		VMLWK_SYMBOL(__start___kcrctab_gpl_future) = .;		\
		*(__kcrctab_gpl_future)					\
		VMLWK_SYMBOL(__stop___kcrctab_gpl_future) = .;		\
	}								\
									\
	/* Kernel symbol table: strings */				\
        __ksymtab_strings : AT(ADDR(__ksymtab_strings) - LOAD_OFFSET) {	\
		*(__ksymtab_strings)					\
	}								\
									\
	/* Built-in module parameters. */				\
	__param : AT(ADDR(__param) - LOAD_OFFSET) {			\
		VMLWK_SYMBOL(__start___param) = .;			\
		*(__param)						\
		VMLWK_SYMBOL(__stop___param) = .;			\
		VMLWK_SYMBOL(__end_rodata) = .;				\
	}								\
									\
	. = ALIGN((align));

/* RODATA provided for backward compatibility.
 * All archs are supposed to use RO_DATA() */
#define RODATA RO_DATA(4096)

#define SECURITY_INIT							\
	.security_initcall.init : AT(ADDR(.security_initcall.init) - LOAD_OFFSET) { \
		VMLWK_SYMBOL(__security_initcall_start) = .;		\
		*(.security_initcall.init) 				\
		VMLWK_SYMBOL(__security_initcall_end) = .;		\
	}

/* .text section. Map to function alignment to avoid address changes
 * during second ld run in second ld pass when generating System.map */
#define TEXT_TEXT							\
		ALIGN_FUNCTION();					\
		*(.text)						\
		*(.text.init.refok)

/* sched.text is aling to function alignment to secure we have same
 * address even at second ld pass when generating System.map */
#define SCHED_TEXT							\
		ALIGN_FUNCTION();					\
		VMLWK_SYMBOL(__sched_text_start) = .;			\
		*(.sched.text)						\
		VMLWK_SYMBOL(__sched_text_end) = .;

/* spinlock.text is aling to function alignment to secure we have same
 * address even at second ld pass when generating System.map */
#define LOCK_TEXT							\
		ALIGN_FUNCTION();					\
		VMLWK_SYMBOL(__lock_text_start) = .;			\
		*(.spinlock.text)					\
		VMLWK_SYMBOL(__lock_text_end) = .;

#define KPROBES_TEXT							\
		ALIGN_FUNCTION();					\
		VMLWK_SYMBOL(__kprobes_text_start) = .;			\
		*(.kprobes.text)					\
		VMLWK_SYMBOL(__kprobes_text_end) = .;

		/* DWARF debug sections.
		Symbols in the DWARF debugging sections are relative to
		the beginning of the section so we begin them at 0.  */
#define DWARF_DEBUG							\
		/* DWARF 1 */						\
		.debug          0 : { *(.debug) }			\
		.line           0 : { *(.line) }			\
		/* GNU DWARF 1 extensions */				\
		.debug_srcinfo  0 : { *(.debug_srcinfo) }		\
		.debug_sfnames  0 : { *(.debug_sfnames) }		\
		/* DWARF 1.1 and DWARF 2 */				\
		.debug_aranges  0 : { *(.debug_aranges) }		\
		.debug_pubnames 0 : { *(.debug_pubnames) }		\
		/* DWARF 2 */						\
		.debug_info     0 : { *(.debug_info			\
				.gnu.linkonce.wi.*) }			\
		.debug_abbrev   0 : { *(.debug_abbrev) }		\
		.debug_line     0 : { *(.debug_line) }			\
		.debug_frame    0 : { *(.debug_frame) }			\
		.debug_str      0 : { *(.debug_str) }			\
		.debug_loc      0 : { *(.debug_loc) }			\
		.debug_macinfo  0 : { *(.debug_macinfo) }		\
		/* SGI/MIPS DWARF 2 extensions */			\
		.debug_weaknames 0 : { *(.debug_weaknames) }		\
		.debug_funcnames 0 : { *(.debug_funcnames) }		\
		.debug_typenames 0 : { *(.debug_typenames) }		\
		.debug_varnames  0 : { *(.debug_varnames) }		\

		/* Stabs debugging sections.  */
#define STABS_DEBUG							\
		.stab 0 : { *(.stab) }					\
		.stabstr 0 : { *(.stabstr) }				\
		.stab.excl 0 : { *(.stab.excl) }			\
		.stab.exclstr 0 : { *(.stab.exclstr) }			\
		.stab.index 0 : { *(.stab.index) }			\
		.stab.indexstr 0 : { *(.stab.indexstr) }		\
		.comment 0 : { *(.comment) }

#define BUG_TABLE							\
	. = ALIGN(8);							\
	__bug_table : AT(ADDR(__bug_table) - LOAD_OFFSET) {		\
		__start___bug_table = .;				\
		*(__bug_table)						\
		__stop___bug_table = .;					\
	}

#define NOTES								\
	.notes : { *(.note.*) } :note

#define INITCALLS							\
  	*(.initcall0.init)						\
  	*(.initcall0s.init)						\
  	*(.initcall1.init)						\
  	*(.initcall1s.init)						\
  	*(.initcall2.init)						\
  	*(.initcall2s.init)						\
  	*(.initcall3.init)						\
  	*(.initcall3s.init)						\
  	*(.initcall4.init)						\
  	*(.initcall4s.init)						\
  	*(.initcall5.init)						\
  	*(.initcall5s.init)						\
	*(.initcallrootfs.init)						\
  	*(.initcall6.init)						\
  	*(.initcall6s.init)						\
  	*(.initcall7.init)						\
  	*(.initcall7s.init)


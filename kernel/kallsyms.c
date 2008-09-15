/*
 * kallsyms.c: in-kernel printing of symbolic oopses and stack traces.
 *
 * Rewritten and vastly simplified by Rusty Russell for in-kernel
 * module loader:
 *   Copyright 2002 Rusty Russell <rusty@rustcorp.com.au> IBM Corporation
 *
 * ChangeLog:
 *
 * (25/Aug/2004) Paulo Marques <pmarques@grupopie.com>
 *      Changed the compression method from stem compression to "table lookup"
 *      compression (see scripts/kallsyms.c for a more complete description)
 */
#include <lwk/kernel.h>
#include <lwk/kallsyms.h>
#include <lwk/init.h>
#include <lwk/string.h>
#include <arch/sections.h>

#ifdef CONFIG_KALLSYMS_ALL
#define all_var 1
#else
#define all_var 0
#endif

/**
 * These will be re-linked against their
 * real values during the second link stage
 */
extern unsigned long kallsyms_addresses[] __attribute__((weak));
extern unsigned long kallsyms_num_syms __attribute__((weak,section("data")));
extern u8 kallsyms_names[] __attribute__((weak));

extern u8 kallsyms_token_table[] __attribute__((weak));
extern u16 kallsyms_token_index[] __attribute__((weak));

extern unsigned long kallsyms_markers[] __attribute__((weak));

static inline int is_kernel_inittext(unsigned long addr)
{
	if (addr >= (unsigned long)_sinittext
	    && addr <= (unsigned long)_einittext)
		return 1;
	return 0;
}

static inline int is_kernel_extratext(unsigned long addr)
{
	if (addr >= (unsigned long)_sextratext
	    && addr <= (unsigned long)_eextratext)
		return 1;
	return 0;
}

static inline int is_kernel_text(unsigned long addr)
{
	if (addr >= (unsigned long)_stext && addr <= (unsigned long)_etext)
		return 1;
	return 0;
}

static inline int is_kernel(unsigned long addr)
{
	if (addr >= (unsigned long)_stext && addr <= (unsigned long)_end)
		return 1;
	return 0;
}

/**
 * Expand a compressed symbol data into the resulting uncompressed string,
 * given the offset to where the symbol is in the compressed stream.
 */
static unsigned int kallsyms_expand_symbol(unsigned int off, char *result)
{
	int len, skipped_first = 0;
	u8 *tptr, *data;

	/* get the compressed symbol length from the first symbol byte */
	data = &kallsyms_names[off];
	len = *data;
	data++;

	/* update the offset to return the offset for the next symbol on
	 * the compressed stream */
	off += len + 1;

	/* for every byte on the compressed symbol data, copy the table
	   entry for that byte */
	while(len) {
		tptr = &kallsyms_token_table[ kallsyms_token_index[*data] ];
		data++;
		len--;

		while (*tptr) {
			if(skipped_first) {
				*result = *tptr;
				result++;
			} else
				skipped_first = 1;
			tptr++;
		}
	}

	*result = '\0';

	/* return to offset to the next symbol */
	return off;
}

/**
 * Find the offset on the compressed stream given and index in the
 * kallsyms array.
 */
static unsigned int get_symbol_offset(unsigned long pos)
{
	u8 *name;
	int i;

	/* use the closest marker we have. We have markers every 256 positions,
	 * so that should be close enough */
	name = &kallsyms_names[ kallsyms_markers[pos>>8] ];

	/* sequentially scan all the symbols up to the point we're searching
	 * for. Every symbol is stored in a [<len>][<len> bytes of data]
	 * format, so we just need to add the len to the current pointer for
	 * every symbol we wish to skip */
	for(i = 0; i < (pos&0xFF); i++)
		name = name + (*name) + 1;

	return name - kallsyms_names;
}

/**
 *  Lookup the address for this symbol. Returns 0 if not found.
 */
unsigned long kallsyms_lookup_name(const char *name)
{
	char namebuf[KSYM_NAME_LEN+1];
	unsigned long i;
	unsigned int off;

	for (i = 0, off = 0; i < kallsyms_num_syms; i++) {
		off = kallsyms_expand_symbol(off, namebuf);

		if (strcmp(namebuf, name) == 0)
			return kallsyms_addresses[i];
	}
	return 0;
}

/**
 * Lookup the symbol name corresponding to a kernel address
 */
const char *kallsyms_lookup(unsigned long addr,
			    unsigned long *symbolsize,
			    unsigned long *offset,
			    char *namebuf)
{
	unsigned long i, low, high, mid;

	/* This kernel should never had been booted. */
	BUG_ON(!kallsyms_addresses);

	namebuf[KSYM_NAME_LEN] = 0;
	namebuf[0] = 0;

	if ((all_var && is_kernel(addr)) ||
	    (!all_var && (is_kernel_text(addr) || is_kernel_inittext(addr) ||
				is_kernel_extratext(addr)))) {
		unsigned long symbol_end = 0;

		/* do a binary search on the sorted kallsyms_addresses array */
		low = 0;
		high = kallsyms_num_syms;

		while (high-low > 1) {
			mid = (low + high) / 2;
			if (kallsyms_addresses[mid] <= addr) low = mid;
			else high = mid;
		}

		/* search for the first aliased symbol. Aliased symbols are
		   symbols with the same address */
		while (low && kallsyms_addresses[low - 1] ==
				kallsyms_addresses[low])
			--low;

		/* Grab name */
		kallsyms_expand_symbol(get_symbol_offset(low), namebuf);

		/* Search for next non-aliased symbol */
		for (i = low + 1; i < kallsyms_num_syms; i++) {
			if (kallsyms_addresses[i] > kallsyms_addresses[low]) {
				symbol_end = kallsyms_addresses[i];
				break;
			}
		}

		/* if we found no next symbol, we use the end of the section */
		if (!symbol_end) {
			if (is_kernel_inittext(addr))
				symbol_end = (unsigned long)_einittext;
			else
				symbol_end = (all_var) ? (unsigned long)_end
				                       : (unsigned long)_etext;
		}

		if (symbolsize)
			*symbolsize = symbol_end - kallsyms_addresses[low];
		if (offset)
			*offset = addr - kallsyms_addresses[low];
		return namebuf;
	}

	return NULL;
}


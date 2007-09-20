#include <lwk/kernel.h>
#include <lwk/init.h>
#include <arch/page.h>

/**
 * Hard coded address of the pointer to the "Extended BIOS Data Area"
 */
#define EBDA_ADDR_POINTER 0x40E

/**
 * Base address and size of the Extended BIOS Data Area.
 */
unsigned long __initdata ebda_addr;
unsigned long __initdata ebda_size;

/**
 * Finds the address and length of the Extended BIOS Data Area.
 */
void __init discover_ebda(void)
{
	/*
	 * There is a real-mode segmented pointer pointing to the 
	 * 4K EBDA area at 0x40E
	 */
	ebda_addr = *(unsigned short *)EBDA_ADDR_POINTER;
	ebda_addr <<= 4;

	ebda_size = *(unsigned short *)(unsigned long)ebda_addr;

	/* Round EBDA up to pages */
	if (ebda_size == 0)
		ebda_size = 1;
	ebda_size <<= 10;
	ebda_size = round_up(ebda_size + (ebda_addr & ~PAGE_MASK), PAGE_SIZE);
	if (ebda_size > 64*1024)
		ebda_size = 64*1024;
}


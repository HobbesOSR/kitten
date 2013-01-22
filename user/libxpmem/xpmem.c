#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <xpmem.h>
#include <lwk/liblwk.h>


/* Pretend we are XPMEM version 2.2 */
int xpmem_version(void)
{
	return 0x00022000;
}


/**
 * On Kitten, the convention is that SMARTMAP slot 0 is used by the PCT
 * and SMARTMAP slots 1 to N are used for app processes 0 to N-1.
 * Furthermore, app processes 0 to N-1 have their pids (process IDs) offset
 * by 0x1000.  This avoids using pids 0 and 1, which are reserved in UNIX.
 */
static unsigned long make_xpmem_addr(pid_t src, void *vaddr)
{
	unsigned long slot;

	if (src == INIT_ASPACE_ID)
		slot = 0;
	else
		slot = src - 0x1000 + 1;

	return (((slot + 1) << SMARTMAP_SHIFT) | ((unsigned long) vaddr));
}


/**
 * On Kitten, xpmem_make simply calculates the address that the source's
 * region will appear in any destinations that map it.  The address is
 * encoded directly in the returned xpmem_segid_t.
 */
xpmem_segid_t xpmem_make(void *vaddr, size_t size, int permit_type, void *permit_value)
{
	return (xpmem_segid_t) make_xpmem_addr(getpid(), vaddr);
}


int xpmem_remove(xpmem_segid_t segid)
{
	/* This is a NOP on Kitten */
	return 0;
}


/**
 * On Kitten, xpmem_get passes the input segid directly through to apid.
 * The segid encodes both the source process id and address.
 */
xpmem_apid_t xpmem_get(xpmem_segid_t segid, int flags, int permit_type, void *permit_value)
{
	return segid;
}


int xpmem_release(xpmem_apid_t apid)
{
	/* This is a NOP on Kitten */
	return 0;
}


/**
 * On Kitten, xpmem_attach simply returns the address specified by addr.
 * The base address of the source process is encoded in addr.apid and the
 * desired address in the source to map is specified in addr.offset.
 */
void *xpmem_attach(struct xpmem_addr addr, size_t size, void *vaddr)
{
	if (vaddr != NULL) {
		printf("%s: ERROR -- Kitten does not support user-specified target vaddr.\n", __FUNCTION__);
		return (void *) -1;
	}

	return (void *)(addr.apid + addr.offset);
}


int xpmem_detach(void *vaddr)
{
	/* This is a NOP on Kitten */
	return 0;
}

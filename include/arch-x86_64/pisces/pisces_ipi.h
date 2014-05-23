#ifndef __PISCES_IPI_H__
#define __PISCES_IPI_H__


/* IPI allocation definitions */

/* TODO: dynamic vector management and ipi redirection table */

/* [64,238] in Kitten is free for use by devices
 * we grab [160, 221] for Pisces
 */


#define PISCES_PCI_IPI_VECTOR   160
#define PISCES_CTRL_IPI_VECTOR  220
#define PISCES_XPMEM_IPI_VECTOR 221


#define PISCES_PCI_NUM_IPIS    (PISCES_CTRL_IPI_VECTOR - PISCES_PCI_IPI_VECTOR)



#endif /* __PISCES_IPI_H__ */

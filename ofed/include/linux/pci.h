/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LINUX_PCI_H_
#define	_LINUX_PCI_H_

#include <linux/types.h>

#include <lwk/pci/pcicfg_regs.h>
#include <lwk/pci/pci.h>
#include <lwk/resource.h>

#include <linux/init.h>
#include <linux/list.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <asm/atomic.h>
#include <linux/device.h>

#if 0
struct pci_device_id {
	uint32_t	vendor;
	uint32_t	device;
        uint32_t	subvendor;
	uint32_t	subdevice;
	uint32_t	class_mask;
	uintptr_t	driver_data;
};
#endif

#define	MODULE_DEVICE_TABLE(bus, table)
#define	PCI_ANY_ID		(-1)
#define	PCI_VENDOR_ID_MELLANOX			0x15b3
#define	PCI_VENDOR_ID_TOPSPIN			0x1867
#define	PCI_DEVICE_ID_MELLANOX_TAVOR		0x5a44
#define	PCI_DEVICE_ID_MELLANOX_TAVOR_BRIDGE	0x5a46
#define	PCI_DEVICE_ID_MELLANOX_ARBEL_COMPAT	0x6278
#define	PCI_DEVICE_ID_MELLANOX_ARBEL		0x6282
#define	PCI_DEVICE_ID_MELLANOX_SINAI_OLD	0x5e8c
#define	PCI_DEVICE_ID_MELLANOX_SINAI		0x6274


#define PCI_VDEVICE(_vendor, _device)					\
	    .vendor = PCI_VENDOR_ID_##_vendor, .device = (_device),	\
	    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#define	PCI_DEVICE(_vendor, _device)					\
	    .vendor = (_vendor), .device = (_device),			\
	    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

#define	to_pci_dev(n)	container_of(n, struct pci_dev, dev)

#define	PCI_VENDOR_ID	PCIR_DEVVENDOR
#define	PCI_COMMAND	PCIR_COMMAND
#define PCI_EXP_DEVCTL  8
#define PCI_EXP_LNKCTL  16


#if 0
extern struct list_head pci_devices;
extern spinlock_t pci_lock;
#endif

#define	__devexit_p(x)	x

#if 0
struct pci_dev {

	struct device		 dev;
    //	struct list_head 	 links;
    //	struct pci_driver      * pdrv;

    uint64_t		 dma_mask; /* only used for internal DMA mapping */
	uint16_t		 device;
	uint16_t		 vendor;
	unsigned int		 irq;
        unsigned int             devfn;

        u8                       revision;

	pci_dev_t                lwk_dev;

    //	pci_dev_t              * lwk_dev;
	pci_bus_t              * bus; // LWK bus structure
	//        struct pci_devinfo     * bus; /* bus this device is on, equivalent to linux struct pci_bus */
};
#endif



#if 0
static inline struct device *
_pci_find_irq_dev(unsigned int irq)
{
	struct pci_dev *pdev;

	spin_lock(&pci_lock);
	list_for_each_entry(pdev, &pci_devices, links) {
		if (irq == pdev->dev.irq)
			break;
		if (irq >= pdev->dev.msix && irq < pdev->dev.msix_max)
			break;
	}
	spin_unlock(&pci_lock);
	if (pdev)
		return &pdev->dev;
	return (NULL);
}
#endif

static inline unsigned long
pci_resource_start(struct pci_dev * dev, 
		   int              bar)
{
    pci_bar_t tmp_bar;
    
    pcicfg_bar_decode(&(dev->cfg), bar, &tmp_bar);

    return bar.address;
}

static inline unsigned long 
pci_resource_len(struct pci_dev * dev,
		 int              bar)
{
    pci_bar_t tmp_bar;

    pcicfg_bar_decode(&(dev->cfg), bar, &tmp_bar);

    return bar.size;
}

/*
 * All drivers just seem to want to inspect the type not flags.
 */
static inline int 
pci_resource_flags(struct pci_dev * dev,
		   int              bar)
{
    int       flags = 0;
    pci_bar_t tmp_bar;

    pcicfg_bar_decode(&(dev->cfg), bar, &tmp_bar);
    
    flags |= (tmp_bar.mem      == 1) ? IORESOURCE_IO       : IORESOURCE_MEM;
    flags |= (tmp_bar.prefetch == 1) ? IORESOURCE_PREFETCH : 0;

    return flags;
}


static inline void *
pci_get_drvdata(struct pci_dev *pdev)
{

	return dev_get_drvdata(&pdev->dev);
}

static inline void
pci_set_drvdata(struct pci_dev *pdev, void *data)
{

	dev_set_drvdata(&pdev->dev, data);
}

static inline int
pci_enable_device(struct pci_dev *pdev)
{
	pci_mmio_enable(pdev->lwk_dev);
	pci_ioport_enable(pdev->lwk_dev);
    
	return (0);
}

static inline void
pci_disable_device(struct pci_dev *pdev)
{
}

static inline int
pci_set_master(struct pci_dev *pdev)
{
    pci_dma_dnable(pdev->lwk_dev);
    return (0);
}

static inline int
pci_request_region(struct pci_dev * pdev,
		   int              bar, 
		   const char     * res_name)
{
	int ret   = 0;
	
	unsigned long res_start = 0;
	unsigned long res_len   = 0;
	int           res_flags = 0;

	if (pci_resource_len(pdev, bar) == 0) {
		return 0;
	}
	
	res_flags = pci_resource_flags(pdev, bar);
	res_start = pci_resource_start(pdev, bar);
	res_len   = pci_resource_len  (pdev, bar);
	

	ret = (res_flags & IORESOURCE_IO)  ? request_region     ( res_start, res_len, res_name ) :
	      (res_flags & IORESOURCE_MEM) ? request_mem_region ( res_start, res_len, res_name ) : 0;

	if (!ret) goto err_out;

	return 0;

 err_out:
	dev_warn(&pdev->dev, "BAR %d: can't reserve %s region [%#llx-%#llx]\n",
		 bar,
		 (pci_resource_flags(pdev, bar) & IORESOURCE_IO) ? "I/O" : "mem",
		 (unsigned long long)pci_resource_start ( pdev, bar ),
		 (unsigned long long)pci_resource_end   ( pdev, bar ) );
	return -EBUSY;   
	
}


static inline void
pci_release_region(struct pci_dev *pdev, int bar)
{
	unsigned long res_start = 0;
	unsigned long res_len   = 0;
	int           res_flags = 0;

	if (pci_resource_len(pdev, bar) == 0) {
		return;
	}

	res_flags = pci_resource_flags(pdev, bar);
	res_start = pci_resource_start(pdev, bar);
	res_len   = pci_resource_len  (pdev, bar);
	
	(res_flags & IORESOURCE_IO)  ? release_region     ( res_start, res_len ) :
	(res_flags & IORESOURCE_MEM) ? release_mem_region ( res_start, res_len ) : ;
}


static inline void
pci_release_regions(struct pci_dev *pdev)
{
	int i;

	for (i = 0; i <= PCIR_MAX_BAR_0; i++)
		pci_release_region(pdev, i);
}

static inline int
pci_request_regions(struct pci_dev *pdev, const char *res_name)
{
	int error;
	int i;

	for (i = 0; i <= PCIR_MAX_BAR_0; i++) {
		error = pci_request_region(pdev, i, res_name);
		if (error && error != -ENODEV) {
			pci_release_regions(pdev);
			return (error);
		}
	}
	return (0);
}



#define	PCI_CAP_ID_EXP	PCIY_EXPRESS
#define	PCI_CAP_ID_PCIX	PCIY_PCIX


#if 0
static inline int
pci_find_capability(struct pci_dev *pdev, int capid)
{
	int reg;

	if (pci_find_cap(pdev->dev.bsddev, capid, &reg))
		return (0);
	return (reg);
}
#endif



/**
 * pci_pcie_cap - get the saved PCIe capability offset
 * @dev: PCI device
 *
 * PCIe capability offset is calculated at PCI device initialization
 * time and saved in the data structure. This function returns saved
 * PCIe capability offset. Using this instead of pci_find_capability()
 * reduces unnecessary search in the PCI configuration space. If you
 * need to calculate PCIe capability offset from raw device for some
 * reasons, please use pci_find_capability() instead.
 */
static inline int pci_pcie_cap(struct pci_dev *dev)
{
        return pci_find_capability(dev, PCI_CAP_ID_EXP);
}



static inline int
pci_read_config_byte(struct pci_dev *pdev, int where, u8 *val)
{
	*val = (u8)pci_read(pdev, where, 1);
	return (0);
}

static inline int
pci_read_config_word(struct pci_dev *pdev, int where, u16 *val)
{
	*val = (u16)pci_read(pdev, where, 2);
	return (0);
}

static inline int
pci_read_config_dword(struct pci_dev *pdev, int where, u32 *val)
{
	*val = (u32)pci_read(pdev, where, 4);
	return (0);
} 

static inline int
pci_write_config_byte(struct pci_dev *pdev, int where, u8 val)
{
	pci_write(pdev, where, 1, val);
	return (0);
}

static inline int
pci_write_config_word(struct pci_dev *pdev, int where, u16 val)
{
	pci_write(pdev, where, 2, val);
	return (0);
}

static inline int
pci_write_config_dword(struct pci_dev *pdev, int where, u32 val)
{ 
	pci_write(pdev, where, 4, val);
	return (0);
}



static struct pci_driver *
linux_pci_find(device_t dev, struct pci_device_id **idp)
{
    return pci_find_driver(dev, idp);
}




/*
struct msix_entry {
	int entry;
	int vector;
};
*/

/*
 * Enable msix, positive errors indicate actual number of available
 * vectors.  Negative errors are failures.
 */
static inline int
pci_enable_msix(struct pci_dev *pdev, struct msix_entry *entries, int nreq)
{
    return pci_msix_setup(pdev, entries, nreq);
}

static inline void
pci_disable_msix(struct pci_dev *pdev)
{
	pci_msix_disable(pdev);
}

static inline int pci_channel_offline(struct pci_dev *pdev)
{
        return false;
}

static inline int pci_enable_sriov(struct pci_dev *dev, int nr_virtfn)
{
        return -ENODEV;
}

static inline void pci_disable_sriov(struct pci_dev *dev)
{
}

/**
 * DEFINE_PCI_DEVICE_TABLE - macro used to describe a pci device table
 * @_table: device table name
 *
 * This macro is used to create a struct pci_device_id array (a device table)
 * in a generic manner.
 */
#define DEFINE_PCI_DEVICE_TABLE(_table) \
	const struct pci_device_id _table[] __devinitdata


/* XXX This should not be necessary. */
#define	pcix_set_mmrbc(d, v)	0
#define	pcix_get_max_mmrbc(d)	0
#define	pcie_set_readrq(d, v)	0

#define	PCI_DMA_BIDIRECTIONAL	0
#define	PCI_DMA_TODEVICE	1
#define	PCI_DMA_FROMDEVICE	2
#define	PCI_DMA_NONE		3

#define	pci_pool		dma_pool
#define pci_pool_destroy	dma_pool_destroy
#define pci_pool_alloc		dma_pool_alloc
#define pci_pool_free		dma_pool_free
#define	pci_pool_create(_name, _pdev, _size, _align, _alloc)		\
	    dma_pool_create(_name, &(_pdev)->dev, _size, _align, _alloc)
#define	pci_free_consistent(_hwdev, _size, _vaddr, _dma_handle)		\
	    dma_free_coherent((_hwdev) == NULL ? NULL : &(_hwdev)->dev,	\
		_size, _vaddr, _dma_handle)
#define	pci_map_sg(_hwdev, _sg, _nents, _dir)				\
	    dma_map_sg((_hwdev) == NULL ? NULL : &(_hwdev->dev),	\
		_sg, _nents, (enum dma_data_direction)_dir)
#define	pci_map_single(_hwdev, _ptr, _size, _dir)			\
	    dma_map_single((_hwdev) == NULL ? NULL : &(_hwdev->dev),	\
		(_ptr), (_size), (enum dma_data_direction)_dir)
#define	pci_unmap_single(_hwdev, _addr, _size, _dir)			\
	    dma_unmap_single((_hwdev) == NULL ? NULL : &(_hwdev)->dev,	\
		_addr, _size, (enum dma_data_direction)_dir)
#define	pci_unmap_sg(_hwdev, _sg, _nents, _dir)				\
	    dma_unmap_sg((_hwdev) == NULL ? NULL : &(_hwdev)->dev,	\
		_sg, _nents, (enum dma_data_direction)_dir)
#define	pci_map_page(_hwdev, _page, _offset, _size, _dir)		\
	    dma_map_page((_hwdev) == NULL ? NULL : &(_hwdev)->dev, _page,\
		_offset, _size, (enum dma_data_direction)_dir)
#define	pci_unmap_page(_hwdev, _dma_address, _size, _dir)		\
	    dma_unmap_page((_hwdev) == NULL ? NULL : &(_hwdev)->dev,	\
		_dma_address, _size, (enum dma_data_direction)_dir)
#define	pci_set_dma_mask(_pdev, mask)	dma_set_mask(&(_pdev)->dev, (mask))
#define	pci_dma_mapping_error(_pdev, _dma_addr)				\
	    dma_mapping_error(&(_pdev)->dev, _dma_addr)
#define	pci_set_consistent_dma_mask(_pdev, _mask)			\
	    dma_set_coherent_mask(&(_pdev)->dev, (_mask))
#define	DECLARE_PCI_UNMAP_ADDR(x)	DEFINE_DMA_UNMAP_ADDR(x);
#define	DECLARE_PCI_UNMAP_LEN(x)	DEFINE_DMA_UNMAP_LEN(x);
#define	pci_unmap_addr		dma_unmap_addr
#define	pci_unmap_addr_set	dma_unmap_addr_set
#define	pci_unmap_len		dma_unmap_len
#define	pci_unmap_len_set	dma_unmap_len_set









#endif	/* _LINUX_PCI_H_ */

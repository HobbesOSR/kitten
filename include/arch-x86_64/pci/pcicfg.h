/* Copyright (c) 2009, Sandia National Laboratories */

#ifndef _ARCH_X86_64_PCICFG_H
#define _ARCH_X86_64_PCICFG_H

uint32_t
arch_pcicfg_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width
);

void
arch_pcicfg_write(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width,
	uint32_t	value
);

int 
raw_pci_read(
	unsigned int    seg, 
	unsigned int    bus,
	unsigned int    devfn, 
	int             reg, 
	int             len,
	u32           * value
);

int 
raw_pci_write(
	unsigned int   seg, 
	 unsigned int   bus,
	 unsigned int   devfn,
	 int            reg, 
	 int            len, 
	 u32            value
);

#endif

/* Copyright (c) 2009, Sandia National Laboratories */

/* 
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
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

#include <lwk/kernel.h>
#include <lwk/pci/pci.h>


/** Pointer to the detailed PCI device database */
static char *	pci_vendordata = NULL;


/** Size of the PCI device database in bytes */
static size_t	pci_vendordata_size;


/** Generic table of PCI device types
 *
 * If no match is found in the detailed PCI device database,
 * this table is used to get general information about the
 * device.
 */
static struct
{
	int	class;
	int	subclass;
	char	*desc;
} pci_nomatch_tab[] = {
	{PCIC_OLD,		-1,			"old"},
	{PCIC_OLD,		PCIS_OLD_NONVGA,	"non-VGA display device"},
	{PCIC_OLD,		PCIS_OLD_VGA,		"VGA-compatible display device"},
	{PCIC_STORAGE,		-1,			"mass storage"},
	{PCIC_STORAGE,		PCIS_STORAGE_SCSI,	"SCSI"},
	{PCIC_STORAGE,		PCIS_STORAGE_IDE,	"ATA"},
	{PCIC_STORAGE,		PCIS_STORAGE_FLOPPY,	"floppy disk"},
	{PCIC_STORAGE,		PCIS_STORAGE_IPI,	"IPI"},
	{PCIC_STORAGE,		PCIS_STORAGE_RAID,	"RAID"},
	{PCIC_STORAGE,		PCIS_STORAGE_ATA_ADMA,	"ATA (ADMA)"},
	{PCIC_STORAGE,		PCIS_STORAGE_SATA,	"SATA"},
	{PCIC_STORAGE,		PCIS_STORAGE_SAS,	"SAS"},
	{PCIC_NETWORK,		-1,			"network"},
	{PCIC_NETWORK,		PCIS_NETWORK_ETHERNET,	"ethernet"},
	{PCIC_NETWORK,		PCIS_NETWORK_TOKENRING,	"token ring"},
	{PCIC_NETWORK,		PCIS_NETWORK_FDDI,	"fddi"},
	{PCIC_NETWORK,		PCIS_NETWORK_ATM,	"ATM"},
	{PCIC_NETWORK,		PCIS_NETWORK_ISDN,	"ISDN"},
	{PCIC_DISPLAY,		-1,			"display"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_VGA,	"VGA"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_XGA,	"XGA"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_3D,	"3D"},
	{PCIC_MULTIMEDIA,	-1,			"multimedia"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_VIDEO,	"video"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_AUDIO,	"audio"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_TELE,	"telephony"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_HDA,	"HDA"},
	{PCIC_MEMORY,		-1,			"memory"},
	{PCIC_MEMORY,		PCIS_MEMORY_RAM,	"RAM"},
	{PCIC_MEMORY,		PCIS_MEMORY_FLASH,	"flash"},
	{PCIC_BRIDGE,		-1,			"bridge"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_HOST,	"HOST-PCI"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_ISA,	"PCI-ISA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_EISA,	"PCI-EISA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_MCA,	"PCI-MCA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_PCI,	"PCI-PCI"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_PCMCIA,	"PCI-PCMCIA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_NUBUS,	"PCI-NuBus"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_CARDBUS,	"PCI-CardBus"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_RACEWAY,	"PCI-RACEway"},
	{PCIC_SIMPLECOMM,	-1,			"simple comms"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_UART,	"UART"},	/* could detect 16550 */
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_PAR,	"parallel port"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_MULSER,	"multiport serial"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_MODEM,	"generic modem"},
	{PCIC_BASEPERIPH,	-1,			"base peripheral"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_PIC,	"interrupt controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_DMA,	"DMA controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_TIMER,	"timer"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_RTC,	"realtime clock"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_PCIHOT,	"PCI hot-plug controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_SDHC,	"SD host controller"},
	{PCIC_INPUTDEV,		-1,			"input device"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_KEYBOARD,	"keyboard"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_DIGITIZER,"digitizer"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_MOUSE,	"mouse"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_SCANNER,	"scanner"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_GAMEPORT,	"gameport"},
	{PCIC_DOCKING,		-1,			"docking station"},
	{PCIC_PROCESSOR,	-1,			"processor"},
	{PCIC_SERIALBUS,	-1,			"serial bus"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_FW,	"FireWire"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_ACCESS,	"AccessBus"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_SSA,	"SSA"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_USB,	"USB"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_FC,	"Fibre Channel"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_SMBUS,	"SMBus"},
	{PCIC_WIRELESS,		-1,			"wireless controller"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_IRDA,	"iRDA"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_IR,	"IR"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_RF,	"RF"},
	{PCIC_INTELLIIO,	-1,			"intelligent I/O controller"},
	{PCIC_INTELLIIO,	PCIS_INTELLIIO_I2O,	"I2O"},
	{PCIC_SATCOM,		-1,			"satellite communication"},
	{PCIC_SATCOM,		PCIS_SATCOM_TV,		"sat TV"},
	{PCIC_SATCOM,		PCIS_SATCOM_AUDIO,	"sat audio"},
	{PCIC_SATCOM,		PCIS_SATCOM_VOICE,	"sat voice"},
	{PCIC_SATCOM,		PCIS_SATCOM_DATA,	"sat data"},
	{PCIC_CRYPTO,		-1,			"encrypt/decrypt"},
	{PCIC_CRYPTO,		PCIS_CRYPTO_NETCOMP,	"network/computer crypto"},
	{PCIC_CRYPTO,		PCIS_CRYPTO_ENTERTAIN,	"entertainment crypto"},
	{PCIC_DASP,		-1,			"dasp"},
	{PCIC_DASP,		PCIS_DASP_DPIO,		"DPIO module"},
	{0, 0,		NULL}
};


/*
 * Parse the PCI device database, if loaded, and return a pointer to a
 * description of the device.
 *
 * The database is flat text formatted as follows:
 *
 * Any line not in a valid format is ignored.
 * Lines are terminated with newline '\n' characters.
 *
 * A VENDOR line consists of the 4 digit (hex) vendor code, a TAB, then
 * the vendor name.
 *
 * A DEVICE line is entered immediately below the corresponding VENDOR ID.
 * - devices cannot be listed without a corresponding VENDOR line.
 * A DEVICE line consists of a TAB, the 4 digit (hex) device code,
 * another TAB, then the device name.
 */

/*
 * Assuming (ptr) points to the beginning of a line in the database,
 * return the vendor or device and description of the next entry.
 * The value of (vendor) or (device) inappropriate for the entry type
 * is set to -1.  Returns nonzero at the end of the database.
 *
 * Note that this is slightly unrobust in the face of corrupt data;
 * we attempt to safeguard against this by spamming the end of the
 * database with a newline when we initialise.
 */

/** Parses a line in the PCI device database. */
static int
pci_db_parse_line(
	char **		ptr,		//!< IN:  pointer to a line in the PCI device database
	int *		vendor,		//!< OUT: the parsed vendor ID
	int *		device,		//!< OUT: the parsed device ID
	char *		desc,		//!< OUT: the parsed device description
	int		desc_width	//!< IN:  size of the desc field in characters
)
{
	char	*cp = *ptr;
	int	left;

	*device = -1;
	*vendor = -1;
	*desc = '\0';
	for (;;) {
		left = pci_vendordata_size - (cp - pci_vendordata);
		if (left <= 0) {
			*ptr = cp;
			return(1);
		}

		/* vendor entry? */
		if (*cp != ';' && *cp != '\t' && sscanf(cp, "%x", vendor) == 1) {
			/* Skip to the start of the vendor description */
			while (*cp != '\t')
				cp++;
			cp++;

			/* now copy until new line encountered */
                        while (*cp && *cp != '\n' && desc_width--) {
                                *desc++ = *cp++;
                        }
                        *desc = '\0';

			break;
		}

		/* device entry? */
		if (*cp == '\t' && sscanf(cp, "%x", device) == 1) {
			/* Skip to the start of the device description */
			cp++;
			while (*cp != '\t')
				cp++;
			cp++;

			/* now copy until new line encountered */
                        while (*cp && *cp != '\n' && desc_width--) {
                                *desc++ = *cp++;
                        }
                        *desc = '\0';

			break;
		}

		/* skip to next line */
		while (*cp != '\n' && left > 0) {
			cp++;
			left--;
		}
		if (*cp == '\n') {
			cp++;
			left--;
		}
	}
	/* skip to next line */
	while (*cp != '\n' && left > 0) {
		cp++;
		left--;
	}
	if (*cp == '\n' && left > 0)
		cp++;
	*ptr = cp;
	return(0);
}


/** Searches for a matching entry in the PCI device database. */
static int
pci_db_lookup(
	pci_dev_t *	dev,	//!< IN:  the device to search for
	size_t		len,	//!< IN:  size of the buffer
	char *		buf	//!< OUT: buffer to store the device's description
)
{
	int	vendor, device;
	char	vp[160];
	char	dp[160];
	char	*line;
	int	found_device = 0;

	/*
	 * If we have no vendor data, we can't do anything.
	 */
	if (pci_vendordata == NULL)
		return -1;

	/*
	 * Scan the vendor data looking for this device
	 */
	line = pci_vendordata;

	/* Look for the start of the entries for the device's vendor */
	for (;;) {
		if (pci_db_parse_line(&line, &vendor, &device, vp, sizeof(vp)))
			return -1; /* ran off end of database */

		if (vendor == dev->cfg.vendor_id) {
			break;
		}
	}

	/* Look for the specific device entry */
	for (;;) {
		if (pci_db_parse_line(&line, &vendor, &device, dp, sizeof(dp)))
			break;  /* ran off end of database */

		if (vendor != -1)
			break;  /* moved on to next vendor's entries */

		if (device == dev->cfg.device_id) {
			found_device = 1;
			break;
		}
	}

	if (found_device == 0)
		snprintf(dp, 80, "0x%x", dev->cfg.device_id);

	snprintf(buf, len, "<%s, %s>", vp, dp);
	return 0;
}


void
pci_describe_device(
	pci_dev_t *	dev,
	size_t		len,
	char *		buf
)
{

#ifdef CONFIG_PCI_DEVICE_DB
	extern int pci_vendors_start, pci_vendors_end;

	pci_vendordata      = (char *)&pci_vendors_start;
	pci_vendordata_size = (unsigned long)&pci_vendors_end -
				(unsigned long)&pci_vendors_start;
#endif

	/*
	 * Look for a listing for this device in a loaded device database.
	 */
	if (pci_db_lookup(dev, len, buf) != -1)
		return;  /* The device was found in the PCI database */

	/*
	 * Fall back to the generic table to get a general description.
	 */
	char *	cp  = "unknown";
	char *	scp = NULL;
	int	i;
	for (i = 0; pci_nomatch_tab[i].desc != NULL; i++) {
		if (pci_nomatch_tab[i].class == dev->cfg.base_class) {
			if (pci_nomatch_tab[i].subclass == -1) {
				cp = pci_nomatch_tab[i].desc;
			} else if (pci_nomatch_tab[i].subclass == dev->cfg.sub_class) {
				scp = pci_nomatch_tab[i].desc;
			}
		}
	}

	snprintf(buf, len, "<%s%s%s>",
		cp ? cp : "",
		((cp != NULL) && (scp != NULL)) ? ", " : "",
		scp ? scp : ""
	);
}

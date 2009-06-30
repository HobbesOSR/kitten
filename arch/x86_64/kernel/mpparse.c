/*
 *	Intel Multiprocessor Specification 1.1 and 1.4
 *	compliant MP-table parsing routines.
 *
 *	(c) 1995 Alan Cox, Building #3 <alan@redhat.com>
 *	(c) 1998, 1999, 2000 Ingo Molnar <mingo@redhat.com>
 *
 *	Fixes
 *		Erich Boleyn	:	MP v1.4 and additional changes.
 *		Alan Cox	:	Added EBDA scanning
 *		Ingo Molnar	:	various cleanups and rewrites
 *		Maciej W. Rozycki:	Bits for default MP configurations
 *		Paul Diefenbaugh:	Added full ACPI support
 */

#include <lwk/smp.h>
#include <lwk/init.h>
#include <lwk/bootmem.h>
#include <lwk/cpuinfo.h>
#include <lwk/params.h>
#include <arch/io.h>
#include <arch/mpspec.h>
#include <arch/proto.h>
#include <arch/io_apic.h>

/**
 * Points to the MP table, once and if it is found.
 * This gets initialized by find_mp_table().
 */
static struct intel_mp_floating *mpf_found;

/**
 * Physical CPU ID of the bootstrap CPU (the BP).
 */
unsigned int __initdata boot_phys_cpu_id = -1U;

/**
 * The number of CPUs in the system.
 */
unsigned int __initdata num_cpus = 0;

/**
 * Map of all CPUs present.
 * Bits set represent physical CPU IDs present.
 */
physid_mask_t phys_cpu_present_map = PHYSID_MASK_NONE;

/**
 * Version information for every Local APIC in the system.
 * The array is indexed by APIC ID.
 */
unsigned char apic_version[MAX_APICS];

/**
 * MP Bus information
 */
static int    mp_current_pci_id = 0;
unsigned char mp_bus_id_to_type    [MAX_MP_BUSSES] = { [0 ... MAX_MP_BUSSES-1] = -1 };
int           mp_bus_id_to_pci_bus [MAX_MP_BUSSES] = { [0 ... MAX_MP_BUSSES-1] = -1 };

/**
 * MP IO APIC information
 */
int nr_ioapics = 0;
struct mpc_config_ioapic mp_ioapics[MAX_IO_APICS];

/* TODO: move these */
int pic_mode;

/**
 * Computes the checksum of an MP configuration block.
 */
static int __init
mpf_checksum(unsigned char *mp, int len)
{
	int sum = 0;
	while (len--)
		sum += *mp++;
	return sum & 0xFF;
}

/**
 * Parses an MP table CPU entry.
 */
static void __init
MP_processor_info(struct mpc_config_processor *m)
{
	int cpu;
	int is_bp;
	unsigned char ver;
	cpumask_t tmp_map;

	if (!(m->mpc_cpuflag & CPU_ENABLED)) {
		printk(KERN_WARNING "A disabled CPU was encountered\n");
		return;
	}

	/* Count the new CPU */
	if (++num_cpus > NR_CPUS)
		panic("NR_CPUS limit of %i reached.\n", NR_CPUS);

	/*
 	 * Determine if this is the bootstrap processor...
	 * the one responsible for booting the other CPUs.
	 */
	is_bp = (m->mpc_cpuflag & CPU_BOOTPROCESSOR);

	/*
	 * Assign a logical CPU ID.
	 * The bootstrap CPU is always assigned logical ID 0.
	 * All other CPUs are assigned the lowest ID available.
	 */
 	if (is_bp) {
		cpu = 0;
	} else {
		cpus_complement(tmp_map, cpu_present_map);
		cpu = first_cpu(tmp_map);
	}

	/* Validate APIC version, fixing up if necessary. */
	ver = m->mpc_apicver;
	if (ver == 0x0) {
		printk(KERN_ERR "BIOS bug, APIC version is 0 for PhysCPU#%d! "
		                "fixing up to 0x10. (tell your hw vendor)\n",
		                m->mpc_apicid);
		ver = 0x10;
	}

	/* Remember the APIC's version */
	apic_version[m->mpc_apicid] = ver;

	/* Add the CPU to the map of physical CPU IDs present. */
	physid_set(m->mpc_apicid, phys_cpu_present_map);

	/* Remember the physical CPU ID of the bootstrap CPU. */
	if (is_bp)
		boot_phys_cpu_id = m->mpc_apicid;

	/* Add the CPU to the map of logical CPU IDs present. */
	cpu_set(cpu, cpu_present_map);

	/* Store ID information. */
	cpu_info[cpu].logical_id   = cpu;
	cpu_info[cpu].physical_id  = m->mpc_apicid;
	cpu_info[cpu].arch.apic_id = m->mpc_apicid;

	printk(KERN_DEBUG
	  "Physical CPU #%d -> Logical CPU #%d, %d:%d APIC version %d%s\n",
		m->mpc_apicid,
		cpu,
	       (m->mpc_cpufeature & CPU_FAMILY_MASK) >> 8,
	       (m->mpc_cpufeature & CPU_MODEL_MASK)  >> 4,
		ver,
		is_bp ? " (Bootstrap CPU)" : "");
}

/**
 * Parses an MP table BUS entry.
 */
static void __init
MP_bus_info(struct mpc_config_bus *m)
{
	char str[7];

	memcpy(str, m->mpc_bustype, 6);
	str[6] = 0;
	printk(KERN_DEBUG "Bus #%d is %s\n", m->mpc_busid, str);

	if (strncmp(str, "ISA", 3) == 0) {
		mp_bus_id_to_type[m->mpc_busid] = MP_BUS_ISA;
	} else if (strncmp(str, "EISA", 4) == 0) {
		mp_bus_id_to_type[m->mpc_busid] = MP_BUS_EISA;
	} else if (strncmp(str, "PCI", 3) == 0) {
		mp_bus_id_to_type[m->mpc_busid] = MP_BUS_PCI;
		mp_bus_id_to_pci_bus[m->mpc_busid] = mp_current_pci_id;
		mp_current_pci_id++;
	} else if (strncmp(str, "MCA", 3) == 0) {
		mp_bus_id_to_type[m->mpc_busid] = MP_BUS_MCA;
	} else {
		printk(KERN_ERR "Unknown bustype %s\n", str);
	}
}

/**
 * Parses an MP table BUS entry.
 */
static void __init
MP_ioapic_info(struct mpc_config_ioapic *m)
{
	if (!(m->mpc_flags & MPC_APIC_USABLE)) {
		printk(KERN_DEBUG "Encountered unusable APIC, ignoring it.\n");
		return;
	}

	printk(KERN_DEBUG "I/O APIC #%d Version %d at 0x%X\n",
		m->mpc_apicid, m->mpc_apicver, m->mpc_apicaddr);
	if (nr_ioapics >= MAX_IO_APICS) {
		printk(KERN_ERR "Max # of I/O APICs (%d) exceeded (found %d).\n",
			MAX_IO_APICS, nr_ioapics);
		panic("Recompile kernel with bigger MAX_IO_APICS!.\n");
	}
	if (!m->mpc_apicaddr) {
		printk(KERN_ERR "WARNING: bogus zero I/O APIC address"
			" found in MP table, skipping!\n");
		return;
	}
	mp_ioapics[nr_ioapics] = *m;
	nr_ioapics++;

	ioapic_info[ioapic_num].phys_id   = m->mpc_apicid;
	ioapic_info[ioapic_num].phys_addr = m->mpc_apicaddr;
	ioapic_num++;
}

/**
 * Parses an MP table IRQ source entry.
 */
static void __init
MP_intsrc_info(struct mpc_config_intsrc *m)
{
	printk(KERN_DEBUG
		"Int: type %d, pol %d, trig %d, bus %d,"
		" BUS IRQ %02d, APIC ID %d, APIC INTIN# %02d\n",
			m->mpc_irqtype, m->mpc_irqflag & 3,
			(m->mpc_irqflag >> 2) & 3, m->mpc_srcbus,
			m->mpc_srcbusirq, m->mpc_dstapic, m->mpc_dstirq);

	struct ioapic_info *ioapic_info = ioapic_info_lookup(m->mpc_dstapic);
	if (!ioapic_info)
		panic("Int entry specifies invalid destination APIC ID.");

	unsigned int delivery_mode;
	switch (m->mpc_irqtype) {
		case 0:  delivery_mode = ioapic_fixed;  break;
		case 1:  delivery_mode = ioapic_NMI;    break;
		case 2:  delivery_mode = ioapic_SMI;    break;
		case 3:  delivery_mode = ioapic_ExtINT; break;
		default: panic("Int entry specifies invalid delivery mode.");
	}

	unsigned int polarity;
	switch (m->mpc_irqflag & 3) {
		case 0:  
			/* Use the default for the source bus type */
			switch (mp_bus_id_to_type[m->mpc_srcbus]) {
				case MP_BUS_ISA:
					polarity = ioapic_active_high;
					break;
				case MP_BUS_PCI:
					polarity = ioapic_active_low;
					break;
				default:
					panic("Default for bus type unknown.");
			}
			break;
		case 1:
			polarity = ioapic_active_high;
			break;
		case 3:
			polarity = ioapic_active_low;
			break;
		default:
			panic("Int entry specifies invalid polarity.");
	}
	
	unsigned int trigger;
	switch ((m->mpc_irqflag >> 2) & 3) {
		case 0:
			/* Use the default for the source bus type */
			switch (mp_bus_id_to_type[m->mpc_srcbus]) {
				case MP_BUS_ISA:
					trigger = ioapic_edge_sensitive;
					break;
				case MP_BUS_PCI:
					trigger = ioapic_level_sensitive;
					break;
				default:
					panic("Default for bus type unknown.");
			}
			break;
		case 1:
			trigger = ioapic_edge_sensitive;
			break;
		case 3:
			trigger = ioapic_level_sensitive;
			break;
		default:
			panic("Int entry specifies invalid trigger mode.");
	}

	if (m->mpc_dstirq >= MAX_IO_APIC_PINS)
		panic("Int entry specifies INTIN# > MAX_IO_APICS.");

	struct ioapic_pin_info *pin_info = &ioapic_info->pin_info[m->mpc_dstirq];

	/*
	 * The first int entry encountered using the pin sets the pin config,
	 * all others must specify the same exact pin config.
	 */
	if (pin_info->num_srcs == 0) {
		pin_info->delivery_mode	=	delivery_mode;
		pin_info->polarity	=	polarity;
		pin_info->trigger	=	trigger;
	} else {
		if (pin_info->delivery_mode != delivery_mode)
			panic("Int entry specifies incompatible delivery_mode.");
		if (pin_info->polarity != polarity)
			panic("Int entry specifies incompatible polarity.");
		if (pin_info->trigger != trigger)
			panic("Int entry specifies incompatible trigger.");
	}

	if (pin_info->num_srcs == MAX_IO_APIC_SRCS)
		panic("Too many devices sharing I/O APIC pin.");

	struct ioapic_src_info *src_info = &pin_info->src_info[pin_info->num_srcs];

	src_info->bus_id	=	m->mpc_srcbus;
	src_info->bus_irq	=	m->mpc_srcbusirq;

	++pin_info->num_srcs;
}

/**
 * Parses an MP table LINT entry.
 */
static void __init
MP_lintsrc_info (struct mpc_config_lintsrc *m)
{
	printk(KERN_DEBUG
		"Lint: type %d, pol %d, trig %d, bus %d,"
		" BUS IRQ %02d, APIC ID %d, APIC LINTIN# %02d\n",
			m->mpc_irqtype, m->mpc_irqflag & 3,
			(m->mpc_irqflag >> 2) &3, m->mpc_srcbusid,
			m->mpc_srcbusirq, m->mpc_destapic, m->mpc_destapiclint);
	/*
	 * Well it seems all SMP boards in existence
	 * use ExtINT/LVT1 == LINT0 and
	 * NMI/LVT2 == LINT1 - the following check
	 * will show us if this assumptions is false.
	 * Until then we do not have to add baggage.
	 */
	if ((m->mpc_irqtype == mp_ExtINT) &&
		(m->mpc_destapiclint != 0))
			BUG();
	if ((m->mpc_irqtype == mp_NMI) &&
		(m->mpc_destapiclint != 1))
			BUG();
}

/**
 * Parses the input MP table, storing various bits of information in global
 * variables as it goes.
 */
static int __init
read_mpc(struct mp_config_table *mpc)
{
	char str[16];
	int count=sizeof(*mpc);
	unsigned char *mpt=((unsigned char *)mpc)+count;

	if (memcmp(mpc->mpc_signature, MPC_SIGNATURE, 4)) {
		printk(KERN_ERR "SMP mptable: bad signature [%c%c%c%c]!\n",
			mpc->mpc_signature[0],
			mpc->mpc_signature[1],
			mpc->mpc_signature[2],
			mpc->mpc_signature[3]);
		return -1;
	}
	if (mpf_checksum((unsigned char *)mpc, mpc->mpc_length)) {
		printk(KERN_ERR "SMP mptable: checksum error!\n");
		return -1;
	}
	if (mpc->mpc_spec!=0x01 && mpc->mpc_spec!=0x04) {
		printk(KERN_ERR "SMP mptable: bad table version (%d)!\n",
			mpc->mpc_spec);
		return -1;
	}
	if (!mpc->mpc_lapic) {
		printk(KERN_ERR "SMP mptable: null local APIC address!\n");
		return -1;
	}
	memcpy(str, mpc->mpc_oem, 8);
	str[8]=0;
	printk(KERN_DEBUG "    OEM ID: %s\n", str);

	memcpy(str, mpc->mpc_productid, 12);
	str[12]=0;
	printk(KERN_DEBUG "    Product ID: %s\n", str);

	printk(KERN_DEBUG "    APIC at: 0x%X\n", mpc->mpc_lapic);

	/* Save the local APIC address, it might be non-default. */
	lapic_phys_addr = mpc->mpc_lapic;

	/* Now process all of the configuration blocks in the table. */
	while (count < mpc->mpc_length) {
		switch(*mpt) {
			case MP_PROCESSOR:
			{
				struct mpc_config_processor *m=
					(struct mpc_config_processor *)mpt;
				MP_processor_info(m);
				mpt += sizeof(*m);
				count += sizeof(*m);
				break;
			}
			case MP_BUS:
			{
				struct mpc_config_bus *m=
					(struct mpc_config_bus *)mpt;
				MP_bus_info(m);
				mpt += sizeof(*m);
				count += sizeof(*m);
				break;
			}
			case MP_IOAPIC:
			{
				struct mpc_config_ioapic *m=
					(struct mpc_config_ioapic *)mpt;
				MP_ioapic_info(m);
				mpt+=sizeof(*m);
				count+=sizeof(*m);
				break;
			}
			case MP_INTSRC:
			{
				struct mpc_config_intsrc *m=
					(struct mpc_config_intsrc *)mpt;

				MP_intsrc_info(m);
				mpt+=sizeof(*m);
				count+=sizeof(*m);
				break;
			}
			case MP_LINTSRC:
			{
				struct mpc_config_lintsrc *m=
					(struct mpc_config_lintsrc *)mpt;
				MP_lintsrc_info(m);
				mpt+=sizeof(*m);
				count+=sizeof(*m);
				break;
			}
		}
	}
	//clustered_apic_check();
	if (!num_cpus)
		printk(KERN_ERR "SMP mptable: no CPUs registered!\n");
	return 0;
}

/**
 * Determines the multiprocessor configuration.
 * The configuration information is stored in global variables so nothing is
 * returned.  find_mp_config() must be called before this function.
 */
void __init
get_mp_config(void)
{
	struct intel_mp_floating *mpf = mpf_found;
	if (!mpf) {
		printk(KERN_WARNING "Assuming 1 CPU.\n");
		num_cpus = 1;
		/* Assign the only CPU logical=physical ID 0 */
		cpu_set(0, cpu_present_map);
		physid_set(0, phys_cpu_present_map);
		cpu_info[0].logical_id   = 0;
		cpu_info[0].physical_id  = 0;
		cpu_info[0].arch.apic_id = 0;
		return;
	}

	printk(KERN_DEBUG "Intel MultiProcessor Specification v1.%d\n",
	       mpf->mpf_specification);
	if (mpf->mpf_feature2 & (1<<7)) {
		printk(KERN_DEBUG "    IMCR and PIC compatibility mode.\n");
		pic_mode = 1;
	} else {
		printk(KERN_DEBUG "    Virtual Wire compatibility mode.\n");
		pic_mode = 0;
	}

	/*
	 * We don't support the default MP configuration.
	 * All supported multi-CPU systems must provide a full MP table.
	 */
	if (mpf->mpf_feature1 != 0)
		BUG();
	if (!mpf->mpf_physptr)
		BUG();

	/*
	 * Set this early so we don't allocate CPU0 if the
	 * MADT list doesn't list the bootstrap processor first.
	 * The bootstrap processor has to be logical ID 0... which
	 * we are reserving here.
	 */
	cpu_set(0, cpu_present_map);

	/*
	 * Parse the MP configuration
	 */
	if (read_mpc(phys_to_virt(mpf->mpf_physptr)))
		panic("BIOS bug, MP table errors detected! (tell your hw vendor)\n");
}

/**
 * Scans a region of memory for the MP table.
 */
static int __init
scan(unsigned long base, unsigned long length)
{
	unsigned int *bp = phys_to_virt(base);
	struct intel_mp_floating *mpf;

	printk(KERN_DEBUG "Scan for MP table from 0x%p for %ld bytes\n",
	                  bp, length);

	while (length > 0) {
		mpf = (struct intel_mp_floating *)bp;
		if ((*bp == SMP_MAGIC_IDENT) &&
			(mpf->mpf_length == 1) &&
			!mpf_checksum((unsigned char *)bp, 16) &&
			((mpf->mpf_specification == 1)
				|| (mpf->mpf_specification == 4)) ) {

			reserve_bootmem(virt_to_phys(mpf), PAGE_SIZE);
			if (mpf->mpf_physptr)
				reserve_bootmem(mpf->mpf_physptr, PAGE_SIZE);
			mpf_found = mpf;
			printk(KERN_DEBUG "Found MP table at:     0x%p\n", mpf);
			return 1;
		}
		bp += 4;
		length -= 16;
	}
	return 0;
}

/**
 * Locates the MP table, if there is one.
 * This does not parse the MP table... get_mp_config() does that.
 */
void __init
find_mp_config(void)
{
	/* 
	 * 1) Scan the bottom 1K for a signature
	 * 2) Scan the top 1K of base RAM
	 * 3) Scan the 64K of bios
	 */
	if (scan(0x0,0x400) ||
		scan(639*0x400,0x400) ||
			scan(0xF0000,0x10000))
		return;

	/*
	 * If it is an MP machine we should know now.
	 *
	 * If not, make a final effort and scan the
	 * Extended BIOS Data Area. 
	 *
	 * NOTE! There are Linux loaders that will corrupt the EBDA
	 * area, and as such this kind of MP config may be less
	 * trustworthy, simply because the MP table may have been
	 * stomped on during early boot. These loaders are buggy and
	 * should be fixed.
	 */
	if (scan(ebda_addr, 0x1000)) {
		printk(KERN_WARNING "MP table found in EBDA\n");
		return;
	}

	/* If we have come this far, we did not find an MP table */
	printk(KERN_DEBUG "No MP table found.\n");
}


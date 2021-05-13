#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>
#include <arch/page.h>
#include <arch/io.h>
#include <arch/sections.h>


#define IORESOURCE_RAM (IORESOURCE_BUSY | IORESOURCE_MEM)

struct resource data_resource = {
	.name = "Kernel data",
	.start = 0,
	.end = 0,
	.flags = IORESOURCE_RAM,
};
struct resource code_resource = {
	.name = "Kernel code",
	.start = 0,
	.end = 0,
	.flags = IORESOURCE_RAM,
};


#define IORESOURCE_ROM (IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM)



extern paddr_t __initdata fdt_start;
extern paddr_t __initdata fdt_end;


static void __init
probe_roms(void)
{
	printk("probe roms\n");
#if 0
	unsigned long start, length, upper;
	unsigned char *rom;
	int	      i;

	/* video rom */
	upper = adapter_rom_resources[0].start;
	for (start = video_rom_resource.start; start < upper; start += 2048) {
		rom = isa_bus_to_virt(start);
		if (!romsignature(rom))
			continue;

		video_rom_resource.start = start;

		/* 0 < length <= 0x7f * 512, historically */
		length = rom[2] * 512;

		/* if checksum okay, trust length byte */
		if (length && romchecksum(rom, length))
			video_rom_resource.end = start + length - 1;

		request_resource(&iomem_resource, &video_rom_resource);
		break;
	}

	start = (video_rom_resource.end + 1 + 2047) & ~2047UL;
	if (start < upper)
		start = upper;

	/* system rom */
	request_resource(&iomem_resource, &system_rom_resource);
	upper = system_rom_resource.start;

	/* check for extension rom (ignore length byte!) */
	rom = isa_bus_to_virt(extension_rom_resource.start);
	if (romsignature(rom)) {
		length = extension_rom_resource.end - extension_rom_resource.start + 1;
		if (romchecksum(rom, length)) {
			request_resource(&iomem_resource, &extension_rom_resource);
			upper = extension_rom_resource.start;
		}
	}

	/* check for adapter roms on 2k boundaries */
	for (i = 0; i < ADAPTER_ROM_RESOURCES && start < upper; start += 2048) {
		rom = isa_bus_to_virt(start);
		if (!romsignature(rom))
			continue;

		/* 0 < length <= 0x7f * 512, historically */
		length = rom[2] * 512;

		/* but accept any length that fits if checksum okay */
		if (!length || start + length > upper || !romchecksum(rom, length))
			continue;

		adapter_rom_resources[i].start = start;
		adapter_rom_resources[i].end = start + length - 1;
		request_resource(&iomem_resource, &adapter_rom_resources[i]);

		start = adapter_rom_resources[i++].end & ~2047UL;
	}
#endif
}

static void __init
probe_fdt()
{
	printk("Probing FDT at %p\n", fdt_start);


}



void __init
init_resources(void)
{
	unsigned int i;

	code_resource.start = virt_to_phys(&_text);
	code_resource.end   = virt_to_phys(&_etext) - 1;
	data_resource.start = virt_to_phys(&_etext);
	data_resource.end   = virt_to_phys(&_edata) - 1;


	probe_fdt();



//	request_resource(&iomem_resource, &video_ram_resource);

	/* request I/O space for devices used on all i[345]86 PCs */
//	for (i = 0; i < STANDARD_IO_RESOURCES; i++)
//		request_resource(&ioport_resource, &standard_io_resources[i]);
}


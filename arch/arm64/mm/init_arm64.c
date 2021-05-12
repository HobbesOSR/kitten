/*
 * Based on arch/arm/mm/init.c
 *
 * Copyright (C) 1995-2005 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <lwk/init.h>

#include <arch/of_fdt.h>
#include <arch/pgtable.h>
#include <arch/sections.h>
#include <arch/byteorder.h>
#include <arch/memblock.h>
#include <arch/io.h>


#if 0
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/swap.h>
#include <linux/bootmem.h>
#include <linux/mman.h>
#include <linux/nodemask.h>
#include <linux/initrd.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/sort.h>
#include <linux/of_fdt.h>

#include <asm/prom.h>
#include <asm/setup.h>
#include <asm/sizes.h>
#include <asm/tlb.h>

#include "mm.h"


phys_addr_t memstart_addr __read_mostly = 0;



static int __init early_initrd(char *p)
{
	unsigned long start, size;
	char *endp;

	start = memparse(p, &endp);
	if (*endp == ',') {
		size = memparse(endp + 1, NULL);

		phys_initrd_start = start;
		phys_initrd_size = size;
	}
	return 0;
}
early_param("initrd", early_initrd);

#define MAX_DMA32_PFN ((4UL * 1024 * 1024 * 1024) >> PAGE_SHIFT)

#if defined(CONFIG_NO_BOOTMEM)
static void __init zone_sizes_init(unsigned long min, unsigned long max)
{
	struct memblock_region *reg;
	unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
	unsigned long max_dma32 = min;

	memset(zone_size, 0, sizeof(zone_size));

#ifdef CONFIG_ZONE_DMA32
	/* 4GB maximum for 32-bit only capable devices */
	max_dma32 = max(min, min(max, MAX_DMA32_PFN));
	zone_size[ZONE_DMA32] = max_dma32 - min;
#endif
	zone_size[ZONE_NORMAL] = max - max_dma32;

	memcpy(zhole_size, zone_size, sizeof(zhole_size));

	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (start >= max)
			continue;
#ifdef CONFIG_ZONE_DMA32
		if (start < max_dma32) {
			unsigned long dma_end = min(end, max_dma32);
			zhole_size[ZONE_DMA32] -= dma_end - start;
		}
#endif
		if (end > max_dma32) {
			unsigned long normal_end = min(end, max);
			unsigned long normal_start = max(start, max_dma32);
			zhole_size[ZONE_NORMAL] -= normal_end - normal_start;
		}
	}

	free_area_init_node(0, zone_size, min, zhole_size);
}
#endif

#ifdef CONFIG_HAVE_ARCH_PFN_VALID
int pfn_valid(unsigned long pfn)
{
	return memblock_is_memory(pfn << PAGE_SHIFT);
}
EXPORT_SYMBOL(pfn_valid);
#endif

#ifndef CONFIG_SPARSEMEM
static void arm64_memory_present(void)
{
}
#else
static void arm64_memory_present(void)
{
	struct memblock_region *reg;

	for_each_memblock(memory, reg)
		memory_present(0, memblock_region_memory_base_pfn(reg),
			       memblock_region_memory_end_pfn(reg));
}
#endif

#endif


#if 0

#if !defined(CONFIG_NO_BOOTMEM)
static void __init arm64_bootmem_init(unsigned long start_pfn,
                                        unsigned long end_pfn)
{
        struct memblock_region *reg;
        unsigned int boot_pages;
        phys_addr_t bitmap;
        pg_data_t *pgdat;

        /*
         * Allocate the bootmem bitmap page.  This must be in a region of
         * memory which has already been mapped.
         */
        boot_pages = bootmem_bootmap_pages(end_pfn - start_pfn);
        bitmap = memblock_alloc_base(boot_pages << PAGE_SHIFT, L1_CACHE_BYTES,
                                __pfn_to_phys(end_pfn));

        /*
         * Initialise the bootmem allocator, handing the memory banks over to
         * bootmem.
         */
        node_set_online(0);
        pgdat = NODE_DATA(0);
        init_bootmem_node(pgdat, __phys_to_pfn(bitmap), start_pfn, end_pfn);

        /* Free the memory regions from memblock into bootmem */
        for_each_memblock(memory, reg) {
                unsigned long start = memblock_region_memory_base_pfn(reg);
                unsigned long end = memblock_region_memory_end_pfn(reg);

                if (end >= end_pfn)
      			end = end_pfn;
                if (start >= end)
                        break;

                free_bootmem(__pfn_to_phys(start), (end - start) << PAGE_SHIFT);
        }

        /* Reserve the memblock reserved regions in bootmem */
        for_each_memblock(reserved, reg) {
                unsigned long start = memblock_region_reserved_base_pfn(reg);
                unsigned long end = memblock_region_reserved_end_pfn(reg);

                if (end >= end_pfn)
                        end = end_pfn;
                if (start >= end)
                        break;

                reserve_bootmem(__pfn_to_phys(start),
                                (end - start) << PAGE_SHIFT, BOOTMEM_DEFAULT);
        }
}

static void __init arm64_bootmem_free(unsigned long min, unsigned long max)
{
        unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
        struct memblock_region *reg;

        /*
         * Initialise the zones.
         */
        memset(zone_size, 0, sizeof(zone_size));

        /*
         * The memory size has already been determined.  If we need to do
         * anything fancy with the allocation of this memory to the zones, now
         * is the time to do it.
         */
        zone_size[0] = max - min;

        /*
         * Calculate the size of the holes.
         *  holes = node_size - sum(bank_sizes)
         */
        memcpy(zhole_size, zone_size, sizeof(zhole_size));
        for_each_memblock(memory, reg) {
                unsigned long start = memblock_region_memory_base_pfn(reg);
                unsigned long end = memblock_region_memory_end_pfn(reg);

                if (start < max) {
                        unsigned long low_end = min(end, max);
                        zhole_size[0] -= low_end - start;
                }
        }

        free_area_init_node(0, zone_size, min, zhole_size);
}

void __init bootmem_init(void)
{
	unsigned long min, max;

	min = PFN_UP(memblock_start_of_DRAM());
	max = PFN_DOWN(memblock_end_of_DRAM());

#if !defined(CONFIG_NO_BOOTMEM)
	arm64_bootmem_init(min, max);
#endif

	/*
	 * Sparsemem tries to allocate bootmem in memory_present(), so must be
	 * done after the fixed reservations.
	 */
	arm64_memory_present();

	sparse_init();
#if !defined(CONFIG_NO_BOOTMEM)
	arm64_bootmem_free(min, max);
#else
	zone_sizes_init(min, max);
#endif
	high_memory = __va((max << PAGE_SHIFT) - 1) + 1;
	max_pfn = max_low_pfn = max;
}
#endif

static inline int free_area(unsigned long pfn, unsigned long end, char *s)
{
	unsigned int pages = 0, size = (end - pfn) << (PAGE_SHIFT - 10);

	for (; pfn < end; pfn++) {
		struct page *page = pfn_to_page(pfn);
		ClearPageReserved(page);
		init_page_count(page);
		__free_page(page);
		pages++;
	}

	if (size && s)
		pr_info("Freeing %s memory: %dK\n", s, size);

	return pages;
}

/*
 * Poison init memory with an undefined instruction (0x0).
 */
static inline void poison_init_mem(void *s, size_t count)
{
	memset(s, 0, count);
}

#ifndef CONFIG_SPARSEMEM_VMEMMAP
static inline void free_memmap(unsigned long start_pfn, unsigned long end_pfn)
{
	struct page *start_pg, *end_pg;
	unsigned long pg, pgend;

	/*
	 * Convert start_pfn/end_pfn to a struct page pointer.
	 */
	start_pg = pfn_to_page(start_pfn - 1) + 1;
	end_pg = pfn_to_page(end_pfn - 1) + 1;

	/*
	 * Convert to physical addresses, and round start upwards and end
	 * downwards.
	 */
	pg = (unsigned long)PAGE_ALIGN(__pa(start_pg));
	pgend = (unsigned long)__pa(end_pg) & PAGE_MASK;

	/*
	 * If there are free pages between these, free the section of the
	 * memmap array.
	 */
	if (pg < pgend)
		free_bootmem(pg, pgend - pg);
}

/*
 * The mem_map array can get very big. Free the unused area of the memory map.
 */
static void __init free_unused_memmap(void)
{
	unsigned long start, prev_end = 0;
	struct memblock_region *reg;

	for_each_memblock(memory, reg) {
		start = __phys_to_pfn(reg->base);

#ifdef CONFIG_SPARSEMEM
		/*
		 * Take care not to free memmap entries that don't exist due
		 * to SPARSEMEM sections which aren't present.
		 */
		start = min(start, ALIGN(prev_end, PAGES_PER_SECTION));
#endif
		/*
		 * If we had a previous bank, and there is a space between the
		 * current bank and the previous, free it.
		 */
		if (prev_end && prev_end < start)
			free_memmap(prev_end, start);

		/*
		 * Align up here since the VM subsystem insists that the
		 * memmap entries are valid from the bank end aligned to
		 * MAX_ORDER_NR_PAGES.
		 */
		prev_end = ALIGN(start + __phys_to_pfn(reg->size),
				 MAX_ORDER_NR_PAGES);
	}

#ifdef CONFIG_SPARSEMEM
	if (!IS_ALIGNED(prev_end, PAGES_PER_SECTION))
		free_memmap(prev_end, ALIGN(prev_end, PAGES_PER_SECTION));
#endif
}
#endif	/* !CONFIG_SPARSEMEM_VMEMMAP */

/*
 * mem_init() marks the free areas in the mem_map and tells us how much memory
 * is free.  This is done after various parts of the system have claimed their
 * memory after the kernel image.
 */
void __init mem_init(void)
{
	unsigned long reserved_pages, free_pages;
	struct memblock_region *reg;

	arm64_swiotlb_init();

	max_mapnr   = pfn_to_page(max_pfn + PHYS_PFN_OFFSET) - mem_map;

#ifndef CONFIG_SPARSEMEM_VMEMMAP
	/* this will put all unused low memory onto the freelists */
	free_unused_memmap();
#endif

	totalram_pages += free_all_bootmem();

	reserved_pages = free_pages = 0;

	for_each_memblock(memory, reg) {
		unsigned int pfn1, pfn2;
		struct page *page, *end;

		pfn1 = __phys_to_pfn(reg->base);
		pfn2 = pfn1 + __phys_to_pfn(reg->size);

		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;

		do {
			if (PageReserved(page))
				reserved_pages++;
			else if (!page_count(page))
				free_pages++;
			page++;
		} while (page < end);
	}

	/*
	 * Since our memory may not be contiguous, calculate the real number
	 * of pages we have in this system.
	 */
	pr_info("Memory:");
	num_physpages = 0;
	for_each_memblock(memory, reg) {
		unsigned long pages = memblock_region_memory_end_pfn(reg) -
			memblock_region_memory_base_pfn(reg);
		num_physpages += pages;
		printk(" %ldMB", pages >> (20 - PAGE_SHIFT));
	}
	printk(" = %luMB total\n", num_physpages >> (20 - PAGE_SHIFT));

	pr_notice("Memory: %luk/%luk available, %luk reserved\n",
		  nr_free_pages() << (PAGE_SHIFT-10),
		  free_pages << (PAGE_SHIFT-10),
		  reserved_pages << (PAGE_SHIFT-10));

#define MLK(b, t) b, t, ((t) - (b)) >> 10
#define MLM(b, t) b, t, ((t) - (b)) >> 20
#define MLK_ROUNDUP(b, t) b, t, DIV_ROUND_UP(((t) - (b)), SZ_1K)

	pr_notice("Virtual kernel memory layout:\n"
		  "    vmalloc : 0x%16lx - 0x%16lx   (%6ld MB)\n"
#ifdef CONFIG_SPARSEMEM_VMEMMAP
		  "    vmemmap : 0x%16lx - 0x%16lx   (%6ld MB)\n"
#endif
		  "    modules : 0x%16lx - 0x%16lx   (%6ld MB)\n"
		  "    memory  : 0x%16lx - 0x%16lx   (%6ld MB)\n"
		  "      .init : 0x%p" " - 0x%p" "   (%6ld kB)\n"
		  "      .text : 0x%p" " - 0x%p" "   (%6ld kB)\n"
		  "      .data : 0x%p" " - 0x%p" "   (%6ld kB)\n",
		  MLM(VMALLOC_START, VMALLOC_END),
#ifdef CONFIG_SPARSEMEM_VMEMMAP
		  MLM((unsigned long)virt_to_page(PAGE_OFFSET),
		      (unsigned long)virt_to_page(high_memory)),
#endif
		  MLM(MODULES_VADDR, MODULES_END),
		  MLM(PAGE_OFFSET, (unsigned long)high_memory),

		  MLK_ROUNDUP(__init_begin, __init_end),
		  MLK_ROUNDUP(_text, _etext),
		  MLK_ROUNDUP(_sdata, _edata));

#undef MLK
#undef MLM
#undef MLK_ROUNDUP

	/*
	 * Check boundaries twice: Some fundamental inconsistencies can be
	 * detected at build time already.
	 */
#ifdef CONFIG_COMPAT
	BUILD_BUG_ON(TASK_SIZE_32			> TASK_SIZE_64);
#endif
	BUILD_BUG_ON(TASK_SIZE_64			> MODULES_VADDR);
	BUG_ON(TASK_SIZE_64				> MODULES_VADDR);

	if (PAGE_SIZE >= 16384 && num_physpages <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get anywhere without
		 * overcommit, so turn it on by default.
		 */
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}

void free_initmem(void)
{
	poison_init_mem(__init_begin, __init_end - __init_begin);
	totalram_pages += free_area(__phys_to_pfn(__pa(__init_begin)),
				    __phys_to_pfn(__pa(__init_end)),
				    "init");
}

#ifdef CONFIG_BLK_DEV_INITRD

static int keep_initrd;

void free_initrd_mem(unsigned long start, unsigned long end)
{
	if (!keep_initrd) {
		poison_init_mem((void *)start, PAGE_ALIGN(end) - start);
		totalram_pages += free_area(__phys_to_pfn(__pa(start)),
					    __phys_to_pfn(__pa(end)),
					    "initrd");
	}
}

static int __init keepinitrd_setup(char *__unused)
{
	keep_initrd = 1;
	return 1;
}

__setup("keepinitrd", keepinitrd_setup);
#endif
#endif

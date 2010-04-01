/*
 * Copyright (c) 2006-2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/pci.h>
#include "wc.h"

#if defined(__i386__) || defined(__x86_64__)

#define MLX4_PAT_MASK	(0xFFFFF8FF)
#define MLX4_PAT_MOD	(0x00000100)
#define MLX4_WC_FLAGS	(_PAGE_PWT)
#define X86_MSR_PAT_OFFSET  0x277

static unsigned int wc_enabled = 0;
static u32 old_pat_lo[NR_CPUS] = {0};
static u32 old_pat_hi[NR_CPUS] = {0};

/*  Returns non-zero if we have a chipset write-combining problem */
static int have_wc_errata(void)
{
	struct pci_dev *dev;
	u8 rev;

	if ((dev = pci_get_class(PCI_CLASS_BRIDGE_HOST << 8, NULL)) != NULL) {
		/*
		 * ServerWorks LE chipsets < rev 6 have problems with
		 * write-combining.
		 */
		if (dev->vendor == PCI_VENDOR_ID_SERVERWORKS &&
		    dev->device == PCI_DEVICE_ID_SERVERWORKS_LE) {
			pci_read_config_byte(dev, PCI_CLASS_REVISION, &rev);
			if (rev <= 5) {
				printk(KERN_INFO "ib_mlx4: Serverworks LE rev < 6"
				       " detected. Write-combining disabled.\n");
				pci_dev_put(dev);
				return -ENOSYS;
			}
		}
		/* Intel 450NX errata # 23. Non ascending cacheline evictions
		   to write combining memory may resulting in data corruption */
		if (dev->vendor == PCI_VENDOR_ID_INTEL &&
		    dev->device == PCI_DEVICE_ID_INTEL_82451NX) {
			printk(KERN_INFO "ib_mlx4: Intel 450NX MMC detected."
			       " Write-combining disabled.\n");
			pci_dev_put(dev);
			return -ENOSYS;
		}
		pci_dev_put(dev);
	}
	return 0;
}

static void rd_old_pat(void *err)
{
	*(int *)err |= rdmsr_safe(X86_MSR_PAT_OFFSET,
				  &old_pat_lo[smp_processor_id()],
				  &old_pat_hi[smp_processor_id()]);
}

static void wr_new_pat(void *err)
{
	u32 new_pat_lo = (old_pat_lo[smp_processor_id()] & MLX4_PAT_MASK) |
			  MLX4_PAT_MOD;

	*(int *)err |= wrmsr_safe(X86_MSR_PAT_OFFSET,
				  new_pat_lo,
				  old_pat_hi[smp_processor_id()]);
}

static void wr_old_pat(void *err)
{
	*(int *)err |= wrmsr_safe(X86_MSR_PAT_OFFSET,
				  old_pat_lo[smp_processor_id()],
				  old_pat_hi[smp_processor_id()]);
}

static int read_and_modify_pat(void)
{
	int ret = 0;

	preempt_disable();
	rd_old_pat(&ret);
	if (!ret)
		smp_call_function(rd_old_pat, &ret, 1);
	if (ret)
		goto out;

	wr_new_pat(&ret);
	if (ret)
		goto out;

	smp_call_function(wr_new_pat, &ret, 1);
	BUG_ON(ret); /* have inconsistent PAT state */
out:
	preempt_enable();
	return ret;
}

static int restore_pat(void)
{
	int ret = 0;

	preempt_disable();
	wr_old_pat(&ret);
	if (!ret) {
		smp_call_function(wr_old_pat, &ret, 1);
		BUG_ON(ret); /* have inconsistent PAT state */
	}

	preempt_enable();
	return ret;
}

int mlx4_enable_wc(void)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	int ret;

	if (wc_enabled)
		return 0;

	if (!cpu_has(c, X86_FEATURE_MSR) ||
	    !cpu_has(c, X86_FEATURE_PAT)) {
		printk(KERN_INFO "ib_mlx4: WC not available"
		       " on this processor\n");
		return -ENOSYS;
	}

	if (have_wc_errata())
		return -ENOSYS;

	if (!(ret = read_and_modify_pat()))
		wc_enabled = 1;
	else
		printk(KERN_INFO "ib_mlx4: failed to enable WC\n");
	return ret ? -EIO  : 0;
}

void mlx4_disable_wc(void)
{
	if (wc_enabled) {
		if (!restore_pat())
			wc_enabled = 0;
		else
			printk(KERN_INFO "ib_mlx4: failed to disable WC\n");
	}
}

pgprot_t pgprot_wc(pgprot_t _prot)
{
	return wc_enabled ? __pgprot(pgprot_val(_prot) | MLX4_WC_FLAGS) :
			    pgprot_noncached(_prot);
}

int mlx4_wc_enabled(void)
{
	return wc_enabled;
}

#elif defined(CONFIG_PPC64)

static unsigned int wc_enabled = 0;

int mlx4_enable_wc(void)
{
	wc_enabled = 1;
	return 0;
}

void mlx4_disable_wc(void)
{
	wc_enabled = 0;
}

pgprot_t pgprot_wc(pgprot_t _prot)
{
	return wc_enabled ? __pgprot((pgprot_val(_prot) | _PAGE_NO_CACHE) &
				     ~(pgprot_t)_PAGE_GUARDED) :
			    pgprot_noncached(_prot);
}

int mlx4_wc_enabled(void)
{
	return wc_enabled;
}

#else	/* !(defined(__i386__) || defined(__x86_64__)) */

int mlx4_enable_wc(void){ return 0; }
void mlx4_disable_wc(void){}

pgprot_t pgprot_wc(pgprot_t _prot)
{
	return pgprot_noncached(_prot);
}

int mlx4_wc_enabled(void)
{
	return 0;
}

#endif


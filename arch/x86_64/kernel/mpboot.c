#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/delay.h>
#include <arch/atomic.h>
#include <arch/apicdef.h>
#include <arch/apic.h>


static atomic_t init_deasserted;


int __init
boot_cpu(int phys_apicid, unsigned int start_rip)
{
	unsigned long send_status, accept_status = 0;
	int maxlvt, num_starts, j;

	printk("Asserting INIT.\n");

	/*
	 * Turn INIT on target chip
	 */
	apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(phys_apicid));

	/*
	 * Send IPI
	 */
	apic_write(APIC_ICR, APIC_INT_LEVELTRIG | APIC_INT_ASSERT
				| APIC_DM_INIT);

	printk("Waiting for send to finish...\n");
	send_status = lapic_wait4_icr_idle_safe();

	mdelay(10);

	printk("Deasserting INIT.\n");

	/* Target chip */
	apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(phys_apicid));

	/* Send IPI */
	apic_write(APIC_ICR, APIC_INT_LEVELTRIG | APIC_DM_INIT);

	printk("Waiting for send to finish...\n");
	send_status = lapic_wait4_icr_idle_safe();

	mb();
	atomic_set(&init_deasserted, 1);

	num_starts = 2;

	/*
	 * Run STARTUP IPI loop.
	 */
	printk("#startup loops: %d.\n", num_starts);

	maxlvt = lapic_get_maxlvt();

	for (j = 1; j <= num_starts; j++) {
		printk("Sending STARTUP #%d.\n",j);
		apic_write(APIC_ESR, 0);
		apic_read(APIC_ESR);
		printk("After apic_write.\n");

		/*
		 * STARTUP IPI
		 */

		/* Target chip */
		apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(phys_apicid));

		/* Boot on the stack */
		/* Kick the second */
		apic_write(APIC_ICR, APIC_DM_STARTUP | (start_rip >> 12));

		/*
		 * Give the other CPU some time to accept the IPI.
		 */
		udelay(300);

		printk("Startup point 1.\n");

		printk("Waiting for send to finish...\n");
		send_status = lapic_wait4_icr_idle_safe();

		/*
		 * Give the other CPU some time to accept the IPI.
		 */
		udelay(200);
		/*
		 * Due to the Pentium erratum 3AP.
		 */
		if (maxlvt > 3) {
			apic_write(APIC_ESR, 0);
		}
		accept_status = (apic_read(APIC_ESR) & 0xEF);
		if (send_status || accept_status)
			break;
	}
	printk("After Startup.\n");

	if (send_status)
		printk(KERN_ERR "APIC never delivered???\n");
	if (accept_status)
		printk(KERN_ERR "APIC delivery error (%lx).\n", accept_status);

	return (send_status | accept_status);
}


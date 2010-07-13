/*
**  Port of Trammel's re-implementation of the ANL selfish benchmark into kernel
**  to meaure interruptions on bare hardware and VMMs.
*/

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <arch/page.h>
#include <arch/tsc.h>

/* Only actually run on the boot CPU for now, but support running
 * independently on multiple CPUs later if need be. */

#define MAX_SAMPLES (1024*5)
uint64_t timestamps[ CONFIG_NR_CPUS ][ MAX_SAMPLES ];
uint64_t deltas[ CONFIG_NR_CPUS ][ MAX_SAMPLES ];

void 
measure_noise(int runtime, uint64_t eps)
{
    int i, sample = 0;
    uint64_t now, delta;
    const uint64_t MHZ = cpu_info[this_cpu].arch.tsc_khz * 1000;

    if (runtime <= 0) runtime = 30;
    if (eps == 0) eps = 100;

    printk( KERN_NOTICE
	    "CPU %d: noise measurement runtime=%d eps=%llu\n",
	    this_cpu, runtime, eps );

    disable_APIC_timer();

    printk( KERN_NOTICE
	    "CPU %d: beginning noise measurement.\n", this_cpu);

    const uint64_t epoch = get_cycles();
    uint64_t start = epoch;
    const uint64_t end = start + runtime * MHZ;

    while( 1 )
    {
        now = get_cycles();
        delta = now - start;
        start = now;

        if( now > end )
            break;

        if( delta < eps )
            continue;

        /* An interruption! */
        timestamps[ this_cpu ][ sample ] = now;
        deltas[ this_cpu ][ sample ] = delta;

        if( ++sample >= MAX_SAMPLES )
            break;
    }

    printk( KERN_NOTICE 
	    "CPU %d: noise measurement got %d samples over %llu cycles\n", 
	    this_cpu, sample, now - epoch );

    enable_APIC_timer();

    printk(KERN_NOTICE "CPU\tTime\tDelta\n");
    for( i=0 ; i<sample ; i++ ) {
        printk( KERN_NOTICE
		"%d\t%llu\t%llu\n", 
		this_cpu, timestamps[ this_cpu ][ i ] - epoch, deltas[ this_cpu ][ i ] );
    }

    return;
}

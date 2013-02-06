#include <lwk/cpuinfo.h>
#include <arch/processor.h>
#include <arch/page.h>

// Impedance match with LWK name changes
#define cpuinfo_x86 cpuinfo
#define cpu_data(cpu) cpu_info[cpu]

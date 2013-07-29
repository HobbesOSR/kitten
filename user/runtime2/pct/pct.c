#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <sys/types.h>
#include <wait.h>
#include <assert.h>

#include <lwk/liblwk.h>
#include <portals4/p4ppe.h>
#include <portals4.h>
#include <portals4_util.h>

#include "pct.h"
#include "pct_internal.h"

#ifdef USING_PIAPI
#include "piapi.h"
#endif

// The linker sets this up to point to the embedded app ELF image
int _binary_pct_rawdata_start __attribute__ ((weak));


// Callback that allocates physical memory for an app being loaded
static paddr_t
alloc_app_pmem(size_t size, size_t alignment, uintptr_t arg)
{
	struct pmem_region result;
	char *name = (char *)arg;

	if (pmem_alloc_umem(size, alignment, &result))
		return (paddr_t) NULL;

	// Mark allocated region with the name passed in
	result.name_is_set = true;
	strncpy(result.name, name, sizeof(result.name));
	pmem_update(&result);
	
	return result.start;
}


// Prints the current physical memory map to the screen
static void
print_pmem_map(void)
{
	struct pmem_region query, result;
	unsigned long rgn_size;
	unsigned long umem_total=0, umem_allocated=0, umem_free=0;
	unsigned long kmem_total=0;
	unsigned long boot_total=0;
	unsigned long init_task_total=0;
	unsigned long initrd_total=0;
	
	int status;

	printf("Physical Memory Map\n");
	printf("===================\n");

	query.start = 0;
	query.end = ULONG_MAX;
	pmem_region_unset_all(&query);


	while ((status = pmem_query(&query, &result)) == 0) {

		rgn_size = result.end - result.start;

		printf("  [%#012lx, %#012lx) %7lu MB  %-10s %-10s\n",
			result.start,
			result.end,
			rgn_size / (1024*1024),
			(result.type_is_set)
				? pmem_type_to_string(result.type)
				: "",
			(result.name_is_set)
				? result.name
				: ((result.allocated_is_set)
					? ((result.allocated) ? "allocated" : "free")
					: ""
				  )
		);

		if (result.type == PMEM_TYPE_UMEM) {
			umem_total += rgn_size;
			if (result.allocated_is_set) {
				if (result.allocated)
					umem_allocated += rgn_size;
				else
					umem_free += rgn_size;
			}
		}

		switch (result.type) {
			case PMEM_TYPE_KMEM:      kmem_total      += rgn_size;  break;
			case PMEM_TYPE_BOOTMEM:   boot_total      += rgn_size;  break;
			case PMEM_TYPE_INIT_TASK: init_task_total += rgn_size;  break;
			case PMEM_TYPE_INITRD:    initrd_total    += rgn_size;  break;
			default: break;
		}

		query.start = result.end;
	}

	assert(status == -ENOENT);

	printf("\n");
	printf("Physical Memory Totals:\n");
	printf("=======================\n");
	printf("  UMEM:      %12lu (%lu allocated, %lu free)\n",
	            umem_total, umem_allocated, umem_free);
	printf("  KMEM:      %12lu\n", kmem_total);
	printf("  INIT_TASK: %12lu\n", init_task_total);
	printf("  INITRD:    %12lu\n", initrd_total);
	printf("  BOOTMEM:   %12lu\n", boot_total);
	printf("\n");
}


// Silly little function to add the PPE "key=val, ..." string to our environment
static int
add_keyvals_to_env(char *env)
{
	char *p, *key, *val;
	int status;

	p = env;
	while (*p != '\0') {
		key = p;
		while (*p != '=') ++p;
		*p = '\0';
		val = ++p;
		while ((*p != ',') && (*p != '\0')) ++p;
		if (*p != '\0') {
			*p = '\0';
			p += 2; // skip over the space
		}
		if ((status = setenv(key, val, 1)) != 0)
			return status;
	}
	return 0;
}


// Initializes Portals, starts progress engine, etc.
static void
portals_init(pct_t *pct)
{
	char env[1024];

	// Create a self-mapping in SMARTMAP slot 0.
	// This is needed for the PPE to work.
	CHECK(aspace_smartmap(pct->aspace_id, pct->aspace_id,
	                      SMARTMAP_ALIGN, SMARTMAP_ALIGN));

	// Start up the Portals4 Progress Engine (PPE)
	CHECK(ppe_run(1000, 1, 1024 * 256));

	// Register with the PPE
	CHECK(ppe_add_kitten_client(getpid(), getuid(),
	                            (void *)SMARTMAP_ALIGN, (void *)SMARTMAP_ALIGN,
	                            sizeof(env), env));
	CHECK(add_keyvals_to_env(env));

	// Get ready for Portals communication
	PTL_CHECK(PtlInit());
	PTL_CHECK(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_PHYSICAL,
	                    PCT_PTL_PID, NULL, NULL, &pct->ni_h));
	PTL_CHECK(PtlGetPhysId(pct->ni_h, &pct->ptl_id));
}


static void
portals_process_init_early(pct_t *pct, process_t *process)
{
	// We do not know what the proc's portals pid will be up front.
	// This will be filled in later when the process registers with us.
	process->ptl_id.phys.nid = PTL_NID_ANY;
	process->ptl_id.phys.pid = PTL_PID_ANY;

	// App will be SMARTMAP'ed into PCT's address sapce at its local index + 1
	CHECK(ppe_add_kitten_client(process->aspace_id,
	                            pct->app.user_id,
	                            (void *)(SMARTMAP_ALIGN +
	                              ((process->local_index+1) * SMARTMAP_ALIGN)),
	                            (void *)SMARTMAP_ALIGN,
	                            sizeof(process->ppe_info),
	                            process->ppe_info));
}


static void
portals_process_init_late(pct_t *pct, process_t *process)
{
	// Map the PCT into the application process.
	// By convention, PCT goes in SMARTMAP slot 0.
	CHECK(aspace_smartmap(pct->aspace_id,
	                      process->aspace_id,
	                      SMARTMAP_ALIGN, SMARTMAP_ALIGN));

	// Map the application process into the PCT.
	// By convention, the app proc goes in SMARTMAP slot local_index+1.
	CHECK(aspace_smartmap(process->aspace_id,
	                      pct->aspace_id,
	                      SMARTMAP_ALIGN +
	                        ((process->local_index+1) * SMARTMAP_ALIGN),
	                      SMARTMAP_ALIGN));
}


// Loads an application, but does not start it executing
static int
app_load(
	pct_t *     pct,
	int         world_size,
	int         universe_size,
	int         local_size,
	id_t        user_id,
	id_t        group_id,
	cpu_set_t   avail_cpus,
	void *      elf_image
)
{
	app_t *app = &pct->app;
	int i, cpu, offset, src, dst, global_rank, global_size, rank, size;
	char env[1024];
	char name[32];
	char * env_ptr;

	app->world_size    = world_size;
	app->universe_size = universe_size;
	app->local_size    = local_size;
	app->user_id       = user_id;
	app->group_id      = group_id;
	app->avail_cpus    = avail_cpus;

	// Allocate a state structure for each application process
	app->procs = (process_t *)MALLOC(local_size * sizeof(process_t));

	// Pre-determine the IDs for each application process
	i = 0;
	for (cpu = 0; (cpu <= CPU_SETSIZE) && (i < local_size); cpu++) {
		if (!CPU_ISSET(cpu, &app->avail_cpus))
			continue;

		app->procs[i].local_index = i;
		app->procs[i].task_id     = 0x1000 + i;
		app->procs[i].aspace_id   = app->procs[i].task_id;
		app->procs[i].cpu_id      = cpu;

		++i;
	}

	global_rank = ((env_ptr = getenv("PMI_RANK")) != NULL) ? atoi(env_ptr) : -1;
	global_size = ((env_ptr = getenv("PMI_SIZE")) != NULL) ? atoi(env_ptr) : -1;

	// Portals early initialization.
	// Must be done before a process's address space is created.
	for (i = 0; i < local_size; i++)
		portals_process_init_early(pct, &app->procs[i]);

	/*************************************************************************/
	printf("Creating address spaces...\n");
	for (i = 0; i < local_size; ++i) {
		rank = (global_rank > -1) ? global_rank : i;
		size = (global_size > -1) ? global_size : local_size;

		app->procs[i].start_state.task_id  = app->procs[i].task_id;
		app->procs[i].start_state.cpu_id   = app->procs[i].cpu_id;
		app->procs[i].start_state.user_id  = app->user_id;
		app->procs[i].start_state.group_id = app->group_id;

		sprintf(app->procs[i].start_state.task_name, "RANK%d", rank);

		// Setup the process's environment.
		// This includes info needed to contact PPE.
		offset = 0;
		offset += sprintf(env + offset, "PMI_RANK=%d, ", rank);
		offset += sprintf(env + offset, "PMI_SIZE=%d, ", size);
		offset += sprintf(env + offset, "%s", app->procs[i].ppe_info);

		sprintf(name, "RANK-%d", rank);

		CHECK(elf_load(elf_image, "app", app->procs[i].aspace_id, VM_PAGE_4KB,
		               (1024 * 1024 * 16),  // heap_size  = 16 MB
		               (1024 * 256),        // stack_size = 256 KB
		               "",                  // argv_str
		               env,                 // envp_str
		               &app->procs[i].start_state,
		               (uintptr_t)name, &alloc_app_pmem));
	}
	printf("    OK\n");
	print_pmem_map();
	/*************************************************************************/

	/*************************************************************************/
	printf("Creating app<->app SMARTMAP mappings...\n");
	for (dst = 0; dst < local_size; dst++) {
		for (src = 0; src < local_size; src++) {
			// SMARTMAP slot 0 is reserved, so offset by one.
			// Process with local index i goes in SMARTMAP slot i+1.
			CHECK(aspace_smartmap(app->procs[src].start_state.aspace_id,
			                      app->procs[dst].start_state.aspace_id,
			                      SMARTMAP_ALIGN + ((src + 1) * SMARTMAP_ALIGN),
			                      SMARTMAP_ALIGN));
		}
	}
	printf("    OK\n");
	/*************************************************************************/

	// Portals late initialization.
	// Must be done after a process's address space is created.
	for (i = 0; i < local_size; i++)
		portals_process_init_late(pct, &app->procs[i]);

	return 0;
}

#ifdef USING_PIAPI
void
piapi_callback( piapi_sample_t *sample )
{
	printf( "PIAPI:\n");
	printf( "\tsample - %u of %u\n", sample->number, sample->total );
	printf( "\ttime   - %f\n", sample->time_sec+sample->time_usec/1000000.0 );
	printf( "\tenergy - %f\n", sample->energy );
}
#endif

int
main(int argc, char *argv[], char *envp[])
{
	volatile vaddr_t elf_image = (vaddr_t) &_binary_pct_rawdata_start;
	pct_t pct;
	cpu_set_t cpuset;
	int num_ranks;
	int cpu, i;

	// Figure out my address space ID
	aspace_get_myid(&pct.aspace_id);

	// Initialize networking
	portals_init(&pct);

	// Figure out how many CPUs are available
	num_ranks = 0;
	printf("Available cpus:\n    ");
	pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
	for (cpu = 0; cpu < CPU_SETSIZE; cpu++) {
		if (CPU_ISSET(cpu, &cpuset)) {
			printf("%d ", cpu);
			++num_ranks;
		}
	}
	printf("\n");

#ifdef USING_PIAPI
	void *cntx = 0x0;
	piapi_init( &cntx, PIAPI_MODE_PROXY, piapi_callback, PIAPI_AGNT_SADDR, PIAPI_AGNT_PORT );
#endif 

	// Load the application into memory, but don't start it executing yet
	app_load(&pct, num_ranks, num_ranks, num_ranks, 1, 1, cpuset, (void *)elf_image);

	pmi_init(&pct, &pct.app, PCT_PMI_PT_INDEX);

	/*************************************************************************/
	printf("Creating tasks...\n");
	for (i = 0; i < pct.app.local_size; i++)
		CHECK(task_create(&pct.app.procs[i].start_state, NULL));
	printf("    OK\n");
	/*************************************************************************/

	printf("DONE LOADING APPLICATION\n");

#ifdef USING_PIAPI
	piapi_collect( cntx, PIAPI_PORT_CPU, 60, 1 );
#endif

	/*************************************************************************/
	printf("ENTERING PORTALS EVENT DISPATCH LOOP\n");
	while (1) {
		ptl_event_t ev;

		// Handle PMI requests from local clients
		PTL_CHECK(PtlEQWait(pct.app.pmi_state.client_rx_eq_h, &ev));

		if (ev.type == PTL_EVENT_PUT)
			CHECK(pmi_process_event(&pct, &pct.app, &ev));

		ptl_queue_process_event(pct.app.pmi_state.client_rx_q, &ev);
	}
	/*************************************************************************/

#ifdef USING_PIAPI
	piapi_destroy( cntx );
#endif
}

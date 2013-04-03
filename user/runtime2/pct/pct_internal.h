#ifndef PCT_INTERNAL_H
#define PCT_INTERNAL_H

#include <sched.h>
#include <lwk/liblwk.h>
#include <portals4.h>


#define PPE_INFO_STRING_SIZE    512


// Aborts program if x returns a non-zero error code
#define CHECK(x) \
	do { \
		int status; \
		if ((status = x) != 0) { \
			fprintf(stderr, "'%s' failed, status=%d\n", #x, status); \
			fprintf(stderr, "In %s, %s() line %u\n", \
					__FILE__, __FUNCTION__, (unsigned int)__LINE__); \
			abort(); \
		} \
	} while (0)


typedef struct pmi_state {
	ptl_pt_index_t  client_pt_index;

	// Portals state for handling client requests
	ptl_queue_t     client_rx_q;
	ptl_handle_eq_t client_rx_eq_h;

	// Portals state for sending responses to clients
	ptl_handle_eq_t client_tx_eq_h;
	ptl_md_t        client_tx_md;
	ptl_handle_md_t client_tx_md_h;
	size_t			client_tx_buf_size;
	char *          client_tx_buf;
} pmi_state_t;


typedef struct process {
	id_t            local_index;     // Local index of the process

	// Misc IDs, many of these get copied to the start_state struct below
	id_t            task_id;
	id_t            aspace_id;
	id_t            cpu_id;

	// This structure tells Kitten how to start a new task executing
	start_state_t   start_state;

	// This string contains information about the Portals Progress Engine (PPE)
	char            ppe_info[PPE_INFO_STRING_SIZE];

	// Portals address of the process
	ptl_process_t   ptl_id;
} process_t;


typedef struct app {
	int             world_size;      // For MPI, size of MPI_COMM_WORLD
	int             universe_size;   // For MPI, world_size + spawn capability

	int             local_size;      // Number of processes running locally
	process_t *     procs;           // Array of descriptors, one per local process

	id_t            user_id;
	id_t            group_id;

	cpu_set_t       avail_cpus;      // Bitmap of CPUs that app procs can run on

	pmi_state_t     pmi_state;       // State needed for app process <-> PCT comm
} app_t;


typedef struct pct {
	id_t            aspace_id;       // The PCT's address space ID
	app_t           app;             // Metadata for app PCT is managing

	ptl_handle_ni_t ni_h;            // The network interface handle the PCT
	                                 //     uses for Portals communication.
	ptl_process_t   ptl_id;          // The PCT's Portals ID
} pct_t;


// PMI server related prototypes
int pmi_init(pct_t *pct, app_t *app, ptl_pt_index_t pt_index);
int pmi_process_event(pct_t *pct, app_t *app, const ptl_event_t *ev);


#endif

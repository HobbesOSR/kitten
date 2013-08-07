/*
 * This is the extremely minimalistic PMI job server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <portals4.h>
#include <portals4_util.h>

#include <pct.h>
#include <pmi_server.h>
#include <simple_pmiutil.h>

#define DELIM ": \t\n"

typedef struct pmi_client {
	ptl_process_t ptl_id;
	pmi_state_t pmi_state;
} pmi_client_t;

static int init_portals(pct_t * pct) {
	PTL_CHECK(PtlInit());
	PTL_CHECK(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_PHYSICAL,
						getpid(), NULL, NULL, &pct->ni_h));
	PTL_CHECK(PtlGetPhysId(pct->ni_h, &pct->ptl_id));
}

static int send_to_app(pmi_client_t * client) {
	ptl_event_t ev;
	ptl_process_t target = client->ptl_id;
	pmi_state_t * state = &client->pmi_state;

	/* Send the message */
	PTL_CHECK(
		ptl_enqueue(target, state->client.pt_index, state->client.tx_md_h,
					0, strlen(state->client.tx_buf) + 1, state->client.tx_eq_h)
	);

	return 0;
}

static int pmi_app_init(
	int client_base_rank, 
	int local_size,
	int world_size,
	int universe_size,
	pmi_client_t * client
)
{
	char buf[PMIU_MAXLINE];
	int status;

	status = snprintf(buf, PMIU_MAXLINE,
					  "cmd=app_init base_rank=%d local_size=%d world_size=%d universe_size=%d\n",
					  client_base_rank, local_size, world_size, universe_size);

	if (status < 0) {
		return -1;
	}

	strcpy(client->pmi_state.client.tx_buf, buf); 
	return send_to_app(client);
}

static uint32_t parse_ipv4_addr(const char * addr) {
	int ipbytes[4];
	sscanf(addr, "%d.%d.%d.%d", &ipbytes[3], &ipbytes[2], &ipbytes[1], &ipbytes[0]);
	return ipbytes[0] | (ipbytes[1] << 8) | (ipbytes[2] << 16) | (ipbytes[3] << 24);
}

int main(int argc, char ** argv) {
	FILE * fp;
	char * line, * token, * token2, * token3;
	size_t line_size, read;
	int num_nodes, global_size, i, num_ranks_on_node, rank_no;
	unsigned int handle_no;
	pct_t fake_pct;
	pmi_client_t * pmi_clients;
	ptl_process_t match_id;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s: <NID/PID filename>\n", *argv);
		return EXIT_FAILURE;
	}

	if ((fp = fopen(*(++argv), "r")) == NULL) {
		fprintf(stderr, "Cannot open file %s: ", *argv);
		perror("");
		return EXIT_FAILURE;
	}

	line_size = 0;
	line = NULL;	

	getline(&line, &line_size, fp);
	token = strtok(line, DELIM);
	token2 = strtok(NULL, DELIM);

	num_nodes = atoi(token);
	global_size = atoi(token2);
	printf("Num nodes: %d, num ranks: %d\n", num_nodes, global_size);

	// Setup a fake pct
	fake_pct.app.user_id = PTL_UID_ANY;
	fake_pct.app.local_size = fake_pct.app.world_size = num_nodes;

	// Initialize portals
	init_portals(&fake_pct);

	// Allocate procs for the app, as if they were all running locally
	if ((fake_pct.app.procs = malloc(sizeof(process_t) * num_nodes)) == NULL) {
		abort();
	}

	// Allocate the pmi clients
	if ((pmi_clients = (pmi_client_t *)malloc(sizeof(pmi_client_t) * num_nodes)) == NULL) {
		abort();
	}

	match_id.phys.nid = PTL_NID_ANY; match_id.phys.pid = PTL_PID_ANY;
	pmi_init(&fake_pct, &fake_pct.app, PCT_PMI_SERVER_PT_INDEX, match_id);

	// OK, at this point, Portals is ready. We need to initialize each client's PMI state
	// and send a Put to each client's PCT
	for (i = 0, rank_no = 0; i < num_nodes; i++) {
		read = getline(&line, &line_size, fp);
		if (read == -1) {
			fprintf(stderr, "Invalid rank information in file...\n");
			return EXIT_FAILURE;
		}

		token = strtok(line, DELIM);
		token2 = strtok(NULL, DELIM);
		token3 = strtok(NULL, DELIM);

		pmi_clients[i].ptl_id.phys.nid = parse_ipv4_addr(token);
		pmi_clients[i].ptl_id.phys.pid = atoi(token2);
		num_ranks_on_node = atoi(token3);

		// Save the node's portals ID
		fake_pct.app.procs[i].ptl_id = pmi_clients[i].ptl_id;

		// Save the pmi state
		pmi_clients[i].pmi_state = fake_pct.app.pmi_state;
		pmi_clients[i].pmi_state.client.pt_index = PCT_PMI_PT_INDEX;

		if (0 != pmi_app_init(rank_no, num_ranks_on_node, global_size, global_size, &pmi_clients[i])) {
			fprintf(stderr, "Failed to initialize app %d\n", i);
			return EXIT_FAILURE;
		}

		rank_no += num_ranks_on_node;
	}

	fake_pct.app.pmi_state.client.pt_index = PCT_PMI_PT_INDEX;
	for (;;) {
		ptl_event_t ev;

		//PTL_CHECK(PtlEQPoll(handles, num_nodes, PTL_TIME_FOREVER, &ev, &handle_no));
		PTL_CHECK(PtlEQWait(fake_pct.app.pmi_state.client.rx_eq_h, &ev));

		if (ev.type == PTL_EVENT_PUT) {
			CHECK(pmi_process_event(&fake_pct, &fake_pct.app, &ev));
		}

		//ptl_queue_process_event(queues[handle_no], &ev);
		ptl_queue_process_event(fake_pct.app.pmi_state.client.rx_q, &ev);

		if (fake_pct.app.local_size == 0) {
			break;
		}
	}

	printf("Application exited on all ranks!\n");
	sleep(3);

	// TODO: Free queue allocations?
	free(fake_pct.app.procs);
	free(pmi_clients);
	free(line);
	fclose(fp);
	return EXIT_SUCCESS;	
}

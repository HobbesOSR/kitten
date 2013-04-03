#include "hydra.h"
#include "pmip.h"
#include "pmip_pmi.h"
#include "pct.h"
#include "pct_internal.h"


#define CLIENT_RXQ_NUM_BLOCKS      2
#define CLIENT_RXQ_BLOCK_SIZE      (PCT_MAX_PMI_MSG * 64)


/* Structure that holds generic PMI state */
struct HYD_pmcd_pmip HYD_pmcd_pmip;


/**
 * Initializes the PCT's PMI state.
 * This gets us setup to receive PMI requests from clients and send responses.
 */
int
pmi_init(pct_t *pct, app_t *app, ptl_pt_index_t pt_index)
{
	pmi_state_t *state = &app->pmi_state;
	ptl_process_t match_id;
	int status;

	state->client_pt_index = pt_index;

	/* Initialize the Portals RX queue for incoming client requests */
	PTL_CHECK(PtlEQAlloc(pct->ni_h, 1024, &state->client_rx_eq_h));

	match_id.phys.nid = pct->ptl_id.phys.nid;
	match_id.phys.pid = PTL_PID_ANY;

	PTL_CHECK(
		ptl_queue_init(pct->ni_h, app->user_id, match_id, pt_index,
		               state->client_rx_eq_h,
		               CLIENT_RXQ_NUM_BLOCKS, CLIENT_RXQ_BLOCK_SIZE,
		               PCT_MAX_PMI_MSG, &state->client_rx_q)
	);

	/* Initialize the Portals state needed to send replies to clients */
	PTL_CHECK(PtlEQAlloc(pct->ni_h, 4, &state->client_tx_eq_h));

	state->client_tx_buf_size = PCT_MAX_PMI_MSG;
	state->client_tx_buf      = MALLOC(state->client_tx_buf_size);

	state->client_tx_md.start     = state->client_tx_buf;
	state->client_tx_md.length    = state->client_tx_buf_size;
	state->client_tx_md.options   = 0;
	state->client_tx_md.eq_handle = state->client_tx_eq_h;
	state->client_tx_md.ct_handle = PTL_CT_NONE;

	PTL_CHECK(PtlMDBind(pct->ni_h, &state->client_tx_md, &state->client_tx_md_h));

	/* Initialize the PMI key value store */
	status = HYD_pmcd_pmi_allocate_kvs(&HYD_pmcd_pmip.local.kvs, 0);
	if (status) {
		fprintf(stderr, "unable to allocate kvs space\n");
		abort();
	}

	return 0;
}


int
pmi_process_event(pct_t *pct, app_t *app, const ptl_event_t *ev)
{
	int status = HYD_SUCCESS;
	char *cmd = NULL;
	char *args[HYD_NUM_TMP_STRINGS] = { 0 };
	struct HYD_pmcd_pmip_pmi_handle *h;

	/* Parse out the command and trailing keyval strings */
	status = HYD_pmcd_pmi_parse_pmi_cmd((char *)ev->start, 1, &cmd, args);
	HYDU_ERR_POP(status, "unable to parse PMI command\n");

	/* Call the handler for the command */
	h = HYD_pmcd_pmip_pmi_v1;
	while (h->handler) {
		if (!strcmp(cmd, h->cmd)) {
			status = h->handler(pct, app, ev, args);
			HYDU_ERR_POP(status, "PMI handler returned error\n");
			goto fn_exit;
		}
		h++;
	}

  fn_exit:
	if (cmd)
		HYDU_FREE(cmd);
	HYDU_free_strlist(args);
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	goto fn_exit;
}

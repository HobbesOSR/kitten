/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *	(C) 2008 by Argonne National Laboratory.
 *		See COPYRIGHT in top-level directory.
 */

#include "pmip_pmi.h"
#include "pmip.h"
#include "pct.h"

static HYD_status
send_cmd_downstream(pmi_state_t *state,
					ptl_process_t target_id,
					const char *cmd)
{
	ptl_event_t tx_ev;

	/* Copy the command to the pre-registered transmit buffer */
	strncpy(state->client.tx_buf, cmd, state->client.tx_buf_size);
	state->client.tx_buf[state->client.tx_buf_size - 1] = '\0';

	/* Send the response to the client */
	PTL_CHECK(
		PtlPut(state->client.tx_md_h, 0, strlen(state->client.tx_buf) + 1,
			   PTL_NO_ACK_REQ, target_id, state->client.pt_index,
			   0, 0, NULL, 0)
	);

	/* Wait for the SEND_END */
	PTL_CHECK(PtlEQWait(state->client.tx_eq_h, &tx_ev));
	PTL_ASSERT(tx_ev.type == PTL_EVENT_SEND);

	return HYD_SUCCESS;
}

static HYD_status
send_cmd_to_server(pmi_state_t *state,
					 ptl_process_t target_id,
					 const char *cmd)
{
	ptl_event_t tx_ev;

	/* Copy the command to the pre-registered transmit buffer */
	strncpy(state->client.tx_buf, cmd, state->client.tx_buf_size);
	state->client.tx_buf[state->client.tx_buf_size - 1] = '\0';

	/* Send the response to the server */
	PTL_CHECK(
		PtlPut(state->client.tx_md_h, 0, strlen(state->client.tx_buf) + 1,
			   PTL_NO_ACK_REQ, target_id, state->server.pt_index,
			   0, 0, NULL, 0)
	);

	/* Wait for the SEND_END */
	PTL_CHECK(PtlEQWait(state->client.tx_eq_h, &tx_ev));
	PTL_ASSERT(tx_ev.type == PTL_EVENT_SEND);

	return HYD_SUCCESS;
}

static HYD_status fn_init(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	int pmi_version, pmi_subversion, task_id, i;
	const char *tmp;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	strtok(args[0], "=");
	pmi_version = atoi(strtok(NULL, "="));
	strtok(args[1], "=");
	pmi_subversion = atoi(strtok(NULL, "="));
	strtok(args[2], "=");
	task_id = atoi(strtok(NULL, "="));

	if (pmi_version == 1 && pmi_subversion <= 1)
		tmp = HYDU_strdup("cmd=response_to_init pmi_version=1 pmi_subversion=1 rc=0\n");
	else if (pmi_version == 2 && pmi_subversion == 0)
		tmp = HYDU_strdup("cmd=response_to_init pmi_version=2 pmi_subversion=0 rc=0\n");
	else		/* PMI version mismatch */
		HYDU_ERR_SETANDJUMP(status, HYD_INTERNAL_ERROR,
							"PMI version mismatch; %d.%d\n", pmi_version, pmi_subversion);

	/* Remember the Portals NID/PID for the process */
	for (i = 0; i < app->local_size; i++) {
		if (task_id == app->procs[i].task_id) {
			app->procs[i].ptl_id.phys.nid = ev->initiator.phys.nid;
			app->procs[i].ptl_id.phys.pid = ev->initiator.phys.pid;
			break;
		}
	}

	status = send_cmd_downstream(&app->pmi_state, ev->initiator, tmp);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(tmp);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

static HYD_status fn_app_init(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	const char *base_rank, *local_size, *world_size, *universe_size;
	struct HYD_pmcd_token *tokens;
	int token_count;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	status = HYD_pmcd_pmi_args_to_tokens(args, &tokens, &token_count);
	HYDU_ERR_POP(status, "unable to convert args to tokens\n");

	base_rank = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "base_rank");
	HYDU_ERR_CHKANDJUMP(status, base_rank == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: base_rank\n");

	local_size = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "local_size");
	HYDU_ERR_CHKANDJUMP(status, local_size == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: local_size\n");

	world_size = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "world_size");
	HYDU_ERR_CHKANDJUMP(status, world_size == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: world_size\n");

	universe_size = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "universe_size");
	HYDU_ERR_CHKANDJUMP(status, universe_size == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: universe_size\n");

	/* Remember the Portals NID/PID for the server */
	app->pmi_state.server.ptl_id.phys.nid = ev->initiator.phys.nid;
	app->pmi_state.server.ptl_id.phys.pid = ev->initiator.phys.pid;

	/* Remember the Portals pt index of the server */
	app->pmi_state.server.pt_index = PCT_PMI_SERVER_PT_INDEX;

	/* Set our local app's information */
	app->base_rank = atoi(base_rank);
	app->local_size = atoi(local_size);
	app->world_size = atoi(world_size);
	app->universe_size = atoi(universe_size);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

static HYD_status fn_get_maxes(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	int i;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	i = 0;
	tmp[i++] = HYDU_strdup("cmd=maxes kvsname_max=");
	tmp[i++] = HYDU_int_to_str(PMI_MAXKVSLEN);
	tmp[i++] = HYDU_strdup(" keylen_max=");
	tmp[i++] = HYDU_int_to_str(PMI_MAXKEYLEN);
	tmp[i++] = HYDU_strdup(" vallen_max=");
	tmp[i++] = HYDU_int_to_str(PMI_MAXVALLEN);
	tmp[i++] = HYDU_strdup("\n");
	tmp[i++] = NULL;

	status = HYDU_str_alloc_and_join(tmp, &cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

static HYD_status fn_get_appnum(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	int i, idx;
	struct HYD_exec *exec;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	/* Get the process index */
	for (i = 0; i < app->local_size; i++)
		if ((ev->initiator.phys.nid == app->procs[i].ptl_id.phys.nid) &&
			(ev->initiator.phys.pid == app->procs[i].ptl_id.phys.pid))
			break;
	idx = i;
	HYDU_ASSERT(idx < app->local_size, status);

	i = 0;
	for (exec = HYD_pmcd_pmip.exec_list; exec; exec = exec->next) {
		i += exec->proc_count;
		if (idx < i)
			break;
	}

	i = 0;
	tmp[i++] = HYDU_strdup("cmd=appnum appnum=");
	//tmp[i++] = HYDU_int_to_str(exec->appnum);
	tmp[i++] = HYDU_int_to_str(0);	/* only support one exec for now , so hard code to 0 */
	tmp[i++] = HYDU_strdup("\n");
	tmp[i++] = NULL;

	status = HYDU_str_alloc_and_join(tmp, &cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

static HYD_status fn_get_my_kvsname(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	int i;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	i = 0;
	tmp[i++] = HYDU_strdup("cmd=my_kvsname kvsname=");
	tmp[i++] = HYDU_strdup(HYD_pmcd_pmip.local.kvs->kvs_name);
	tmp[i++] = HYDU_strdup("\n");
	tmp[i++] = NULL;

	status = HYDU_str_alloc_and_join(tmp, &cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

static HYD_status fn_get_usize(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	int i;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	i = 0;
	tmp[i++] = HYDU_strdup("cmd=universe_size size=");
	tmp[i++] = HYDU_int_to_str(app->universe_size);
	tmp[i++] = HYDU_strdup("\n");
	tmp[i++] = NULL;

	status = HYDU_str_alloc_and_join(tmp, &cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

static HYD_status fn_put(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	int i, ret;
	char *kvsname, *key, *val;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	struct HYD_pmcd_token *tokens;
	int token_count;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	status = HYD_pmcd_pmi_args_to_tokens(args, &tokens, &token_count);
	HYDU_ERR_POP(status, "unable to convert args to tokens\n");

	kvsname = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "kvsname");
	HYDU_ERR_CHKANDJUMP(status, kvsname == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: kvsname\n");

	key = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "key");
	HYDU_ERR_CHKANDJUMP(status, key == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: key\n");

	val = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "value");
	if (val == NULL) {
		/* the user sent an empty string */
		val = HYDU_strdup("");
	}

	if (strcmp(HYD_pmcd_pmip.local.kvs->kvs_name, kvsname))
		HYDU_ERR_SETANDJUMP(status, HYD_INTERNAL_ERROR,
							"kvsname (%s) does not match this group's kvs space (%s)\n",
							kvsname, HYD_pmcd_pmip.local.kvs->kvs_name);

	status = HYD_pmcd_pmi_add_kvs(key, val, HYD_pmcd_pmip.local.kvs, &ret);
	HYDU_ERR_POP(status, "unable to add keypair to kvs\n");

	i = 0;
	tmp[i++] = HYDU_strdup("cmd=put_result rc=");
	tmp[i++] = HYDU_int_to_str(ret);
	if (ret == 0) {
		tmp[i++] = HYDU_strdup(" msg=success");
	}
	else {
		tmp[i++] = HYDU_strdup(" msg=duplicate_key");
		tmp[i++] = HYDU_strdup(key);
	}
	tmp[i++] = HYDU_strdup("\n");
	tmp[i++] = NULL;

	status = HYDU_str_alloc_and_join(tmp, &cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYD_pmcd_pmi_free_tokens(tokens, token_count);
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}


static HYD_status fn_get(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	struct HYD_pmcd_pmi_kvs_pair *run;
	char *kvsname, *key, *val;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	struct HYD_pmcd_token *tokens;
	int token_count, i;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	status = HYD_pmcd_pmi_args_to_tokens(args, &tokens, &token_count);
	HYDU_ERR_POP(status, "unable to convert args to tokens\n");

	kvsname = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "kvsname");
	HYDU_ERR_CHKANDJUMP(status, kvsname == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: kvsname\n");

	key = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "key");
	HYDU_ERR_CHKANDJUMP(status, key == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: key\n");

	val = NULL;
	if (!strcmp(key, "PMI_dead_processes")) {
		val = 0;  /* FIXME: return actual number of dead processes */
		goto found_val;
	}

	/* Make sure the key value store name is what we expect */
	if (strcmp(HYD_pmcd_pmip.local.kvs->kvs_name, kvsname))
		HYDU_ERR_SETANDJUMP(status, HYD_INTERNAL_ERROR,
							"kvsname (%s) does not match this group's kvs space (%s)\n",
							kvsname, HYD_pmcd_pmip.local.kvs->kvs_name);

	/* Try to find the value associated with the key */
	for (run = HYD_pmcd_pmip.local.kvs->key_pair; run; run = run->next) {
		if (!strcmp(run->key, key)) {
			val = run->val;
			break;
		}
	}

found_val:
	i = 0;
	if (val) {
		tmp[i++] = HYDU_strdup("cmd=get_result rc=");
		tmp[i++] = HYDU_strdup("0 msg=success value=");
		tmp[i++] = HYDU_strdup(val);
		tmp[i++] = HYDU_strdup("\n");
		tmp[i++] = NULL;

		status = HYDU_str_alloc_and_join(tmp, &cmd);
		HYDU_ERR_POP(status, "unable to join strings\n");
		HYDU_free_strlist(tmp);

		/* Send result to app */
		status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
		HYDU_ERR_POP(status, "error sending PMI response\n");
		HYDU_FREE(cmd);
	} else {
		/* Need to send the request to the server */
		tmp[i++] = HYDU_strdup("cmd=get_server kvsname=");
		tmp[i++] = HYDU_strdup(kvsname);
		tmp[i++] = HYDU_strdup(" key=");
		tmp[i++] = HYDU_strdup(key);
		tmp[i++] = HYDU_strdup(" nid=");
		tmp[i++] = HYDU_int_to_str(ev->initiator.phys.nid);
		tmp[i++] = HYDU_strdup(" pid=");
		tmp[i++] = HYDU_int_to_str(ev->initiator.phys.pid);
		tmp[i++] = HYDU_strdup("\n");
		tmp[i++] = NULL;

		status = HYDU_str_alloc_and_join(tmp, &cmd);
		HYDU_ERR_POP(status, "unable to join strings\n");
		HYDU_free_strlist(tmp);
		
		/* Send get to server */
		status = send_cmd_to_server(&app->pmi_state, app->pmi_state.server.ptl_id, cmd);
		HYDU_ERR_POP(status, "error sending PMI response\n");
		HYDU_FREE(cmd);
	}

  fn_exit:
	HYD_pmcd_pmi_free_tokens(tokens, token_count);
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

/* Master has received a get request */
static HYD_status fn_get_server(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	struct HYD_pmcd_pmi_kvs_pair *run;
	char *kvsname, *key, *val, *nid, *pid;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	struct HYD_pmcd_token *tokens;
	int token_count, i;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	status = HYD_pmcd_pmi_args_to_tokens(args, &tokens, &token_count);
	HYDU_ERR_POP(status, "unable to convert args to tokens\n");

	kvsname = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "kvsname");
	HYDU_ERR_CHKANDJUMP(status, kvsname == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: kvsname\n");

	key = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "key");
	HYDU_ERR_CHKANDJUMP(status, key == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: key\n");

	nid = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "nid");
	HYDU_ERR_CHKANDJUMP(status, nid == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: nid\n");

	pid = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "pid");
	HYDU_ERR_CHKANDJUMP(status, pid == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: pid\n");

	/* Make sure the key value store name is what we expect */
	if (strcmp(HYD_pmcd_pmip.local.kvs->kvs_name, kvsname))
		HYDU_ERR_SETANDJUMP(status, HYD_INTERNAL_ERROR,
							"kvsname (%s) does not match this group's kvs space (%s)\n",
							kvsname, HYD_pmcd_pmip.local.kvs->kvs_name);

	val = NULL;
	if (!strcmp(key, "PMI_dead_processes")) {
		val = 0;  /* FIXME: return actual number of dead processes */
		goto found_val;
	}

	/* Try to find the value associated with the key */
	for (run = HYD_pmcd_pmip.local.kvs->key_pair; run; run = run->next) {
		if (!strcmp(run->key, key)) {
			val = run->val;
			break;
		}
	}

found_val:
	i = 0;
	tmp[i++] = HYDU_strdup("cmd=get_server_result rc=");
	if (val) {
		tmp[i++] = HYDU_strdup("0 msg=success key=");
		tmp[i++] = HYDU_strdup(key);
		tmp[i++] = HYDU_strdup(" value=");
		tmp[i++] = HYDU_strdup(val);
	} else {
		tmp[i++] = HYDU_strdup("-1 msg=key_");
		tmp[i++] = HYDU_strdup(key);
		tmp[i++] = HYDU_strdup("_not_found value=unknown");
	}
	tmp[i++] = HYDU_strdup(" nid=");
	tmp[i++] = HYDU_strdup(nid);
	tmp[i++] = HYDU_strdup(" pid=");
	tmp[i++] = HYDU_strdup(pid);
	tmp[i++] = HYDU_strdup("\n");
	tmp[i++] = NULL;

	status = HYDU_str_alloc_and_join(tmp, &cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYD_pmcd_pmi_free_tokens(tokens, token_count);
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

/* PCT has received a get_server response */
static HYD_status fn_get_server_result(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	char *msg, *key, *value, *nid, *pid;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	struct HYD_pmcd_token *tokens;
	int token_count, i, found, ret;
	ptl_process_t target;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	status = HYD_pmcd_pmi_args_to_tokens(args, &tokens, &token_count);
	HYDU_ERR_POP(status, "unable to convert args to tokens\n");

	msg = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "msg");
	HYDU_ERR_CHKANDJUMP(status, msg == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: msg\n");

	key = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "key");
	HYDU_ERR_CHKANDJUMP(status, key == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: key\n");

	value = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "value");
	HYDU_ERR_CHKANDJUMP(status, value == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: value\n");

	nid = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "nid");
	HYDU_ERR_CHKANDJUMP(status, nid == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: nid\n");

	pid = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "pid");
	HYDU_ERR_CHKANDJUMP(status, pid == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: pid\n");

	/* Did the server find it */
	found = (strcmp(value, "unknown") != 0);

	i = 0;
	tmp[i++] = HYDU_strdup("cmd=get_result rc=");
	if (found) {
		tmp[i++] = HYDU_strdup("0 msg=");
	}
	else {
		tmp[i++] = HYDU_strdup("-1 msg=");
	}
	tmp[i++] = HYDU_strdup(msg);
	tmp[i++] = HYDU_strdup(" value=");
	tmp[i++] = HYDU_strdup(value);
	tmp[i++] = HYDU_strdup("\n");
	tmp[i++] = NULL;

	target.phys.nid = atoi(nid);
	target.phys.pid = atoi(pid);

	status = HYDU_str_alloc_and_join(tmp, &cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	status = send_cmd_downstream(&app->pmi_state, target, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

	/* Store value locally, if we found it */
	if (found) {
		status = HYD_pmcd_pmi_add_kvs(key, value, HYD_pmcd_pmip.local.kvs, &ret);
		HYDU_ERR_POP(status, "unable to add keypair to kvs\n");
	}

  fn_exit:
	HYD_pmcd_pmi_free_tokens(tokens, token_count);
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}


static HYD_status fn_barrier_out(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	const char *cmd;
	int i;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	cmd = HYDU_strdup("cmd=barrier_out\n");

	for (i = 0; i < app->local_size; i++) {
		status = send_cmd_downstream(&app->pmi_state, app->procs[i].ptl_id, cmd);
		HYDU_ERR_POP(status, "error sending PMI response\n");
	}

	HYDU_FREE(cmd);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

static HYD_status fn_barrier_in(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	int i;
	static int barrier_count = 0;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	barrier_count++;
	if (barrier_count < app->local_size) {
		goto fn_exit;
	}

	/* All local apps are here - tell server */
	barrier_count = 0;

	i = 0;
	tmp[i++] = HYDU_strdup("cmd=barrier_server\n");
	tmp[i++] = NULL;

	status = HYDU_str_alloc_and_join(tmp, &cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	/* Send barrier to server */
	status = send_cmd_to_server(&app->pmi_state, app->pmi_state.server.ptl_id, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}

static HYD_status fn_barrier_server(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	static int barrier_count = 0;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	barrier_count++;
	if (barrier_count == app->world_size) {
		barrier_count = 0;

		/* Release everyone from the barrier */
		fn_barrier_out(pct, app, NULL, NULL);
	}

	HYDU_FUNC_EXIT();
	return status;
}

static HYD_status fn_commit(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	char *kvsname;
	char *tmp[HYD_NUM_TMP_STRINGS], *cmd, *kvs_buf, *launch_cmd;
	struct HYD_pmcd_token *tokens;
	static int commit_count = 0;
	int token_count;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	status = HYD_pmcd_pmi_args_to_tokens(args, &tokens, &token_count);
	HYDU_ERR_POP(status, "unable to convert args to tokens\n");

	kvsname = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "kvsname");
	HYDU_ERR_CHKANDJUMP(status, kvsname == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: kvsname\n");

	/* Make sure the key value store name is what we expect */
	if (strcmp(HYD_pmcd_pmip.local.kvs->kvs_name, kvsname))
		HYDU_ERR_SETANDJUMP(status, HYD_INTERNAL_ERROR,
							"kvsname (%s) does not match this group's kvs space (%s)\n",
							kvsname, HYD_pmcd_pmip.local.kvs->kvs_name);

	commit_count++;
	if (commit_count < app->local_size) {
		goto out;
	}
	
	/* Everyone locally has committed - send whole commit to server */
	commit_count = 0;

	/* Construct a string of all of our key-val pairs, and send it to the server */
	/* Try to find the value associated with the key */
	status = HYD_pmcd_pmi_get_kvs_string(HYD_pmcd_pmip.local.kvs, &kvs_buf);
	
	if (status)
		goto fn_fail;

	/* Prepare commit */
	tmp[0] = HYDU_strdup("cmd=commit_server kvsname=");
	tmp[1] = HYDU_strdup(kvsname);
	tmp[2] = HYDU_strdup(" kvs=");
	tmp[3] = HYDU_strdup(kvs_buf);
	tmp[4] = HYDU_strdup("\n");
	tmp[5] = NULL;
	HYDU_FREE(kvs_buf);
	
	status = HYDU_str_alloc_and_join(tmp, &launch_cmd);
	HYDU_ERR_POP(status, "unable to join strings\n");
	HYDU_free_strlist(tmp);

	/* Send commit to server */
	status = send_cmd_to_server(&app->pmi_state, app->pmi_state.server.ptl_id, launch_cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(launch_cmd);

out:
	/* Send result to app */
	cmd = HYDU_strdup("cmd=commit_result\n");
	status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYD_pmcd_pmi_free_tokens(tokens, token_count);
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();

}

static HYD_status fn_commit_server(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	HYD_status status = HYD_SUCCESS;
	struct HYD_pmcd_token *tokens;
	struct HYD_pmcd_pmi_kvs *kvs;
	struct HYD_pmcd_pmi_kvs_pair *run;
	int token_count, ret;
	static int commit_count_server = 0;
	char *kvsname, *kvs_list;

	HYDU_FUNC_ENTER();

	status = HYD_pmcd_pmi_args_to_tokens(args, &tokens, &token_count);
	HYDU_ERR_POP(status, "unable to convert args to tokens\n");

	kvsname = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "kvsname");
	HYDU_ERR_CHKANDJUMP(status, kvsname == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: kvsname\n");

	kvs_list = HYD_pmcd_pmi_find_token_keyval(tokens, token_count, "kvs");
	HYDU_ERR_CHKANDJUMP(status, kvs_list == NULL, HYD_INTERNAL_ERROR,
						"unable to find token: kvs\n");

	/* Allocate a temporary kvs structure for the client information */
	status = HYD_pmcd_pmi_get_kvs_struct(kvs_list, &kvs);

	/* If this is the first commit, save the name - otherwise, make sure the name matches
	 * what has already been committed */
	if (commit_count_server == 0) {
		HYDU_strncpy(HYD_pmcd_pmip.local.kvs->kvs_name, kvsname, PMI_MAXKVSLEN);
	} else {
		HYDU_ERR_CHKANDJUMP(status, strcmp(HYD_pmcd_pmip.local.kvs->kvs_name, kvsname) != 0,
							HYD_INTERNAL_ERROR, "received invalid kvsname in client commit\n");
	}
	commit_count_server++;

	for (run = kvs->key_pair; run; run = run->next) {
		status = HYD_pmcd_pmi_add_kvs(run->key, run->val, HYD_pmcd_pmip.local.kvs, &ret);
		HYDU_ERR_POP(status, "unable to add keypair to kvs\n");
	}
	HYDU_FREE(kvs);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}


static HYD_status fn_finalize(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	const char *cmd;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	/* Tell the server we're done */
	cmd = HYDU_strdup("cmd=finalize_server\n");
	status = send_cmd_to_server(&app->pmi_state, app->pmi_state.server.ptl_id, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

	/* Respond to app */
	cmd = HYDU_strdup("cmd=finalize_ack\n");
	status = send_cmd_downstream(&app->pmi_state, ev->initiator, cmd);
	HYDU_ERR_POP(status, "error sending PMI response\n");
	HYDU_FREE(cmd);

  fn_exit:
	HYDU_FUNC_EXIT();
	return status;

  fn_fail:
	abort();
}


static HYD_status fn_finalize_server(pct_t *pct, app_t *app, const ptl_event_t *ev, char *args[])
{
	static int finalize_count = 0;
	HYD_status status = HYD_SUCCESS;

	HYDU_FUNC_ENTER();

	finalize_count++;
	if (finalize_count == app->world_size) {
		finalize_count = 0;
		app->world_size = app->universe_size = app->local_size = 0;
	}

	HYDU_FUNC_EXIT();
	return status;
}

static struct HYD_pmcd_pmip_pmi_handle pmi_v1_handle_fns_foo[] = {
	{"init", fn_init},
	{"app_init", fn_app_init},
	{"get_maxes", fn_get_maxes},
	{"get_appnum", fn_get_appnum},
	{"get_my_kvsname", fn_get_my_kvsname},
	{"get_universe_size", fn_get_usize},
	{"put", fn_put},
	{"get", fn_get},
	{"get_server", fn_get_server},
	{"get_server_result", fn_get_server_result},
	{"barrier_in", fn_barrier_in},
	{"barrier_out", fn_barrier_out},
	{"barrier_server", fn_barrier_server},
	{"commit", fn_commit},
	{"commit_server", fn_commit_server},
	{"finalize", fn_finalize},
	{"finalize_server", fn_finalize_server},
	{"\0", NULL}
};

struct HYD_pmcd_pmip_pmi_handle *HYD_pmcd_pmip_pmi_v1 = pmi_v1_handle_fns_foo;

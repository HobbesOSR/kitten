/* -*- Mode: C; c-basic-offset:4 ; -*- */

/*
 * This is the PMI client for the Kitten lightweight kernel.
 * It uses Portals4 to communicate with the Kitten PCT (the PMI server).
 * The code is based on simple_pmi from Argonne, updated to use Portals4.
 */

/*  
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/*********************** PMI implementation ********************************/
/*
 * This file implements the client-side of the PMI interface.
 *
 * Note that the PMI client code must not print error messages (except 
 * when an abort is required) because MPI error handling is based on
 * reporting error codes to which messages are attached.  
 *
 * In v2, we should require a PMI client interface to use MPI error codes
 * to provide better integration with MPICH2.  
 */
/***************************************************************************/

#define PMI_VERSION    1
#define PMI_SUBVERSION 1

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <portals4.h>
#include <portals4_util.h>

#include <pct.h>

#include "pmi.h"
#include "simple_pmiutil.h"


#ifndef MPI_MAX_PORT_NAME
#define MPI_MAX_PORT_NAME 128
#endif


/* Set PMI_initialized to 1 for singleton init but no process manager
 * to help.  Initialized to 2 for normal initialization.  Initialized
 * to values higher than 2 when singleton_init by a process manager.
 * All values higher than 1 invlove a PM in some way. */
typedef enum {
	PMI_UNINITIALIZED = 0, 
	NORMAL_INIT_WITH_PM = 2,
} PMIState;


/* PMI client state */
static PMIState PMI_initialized = PMI_UNINITIALIZED;
static int      PMI_size = -1;
static int      PMI_rank = -1;
static int      PMI_kvsname_max = 0;
static int      PMI_keylen_max = 0;
static int      PMI_vallen_max = 0;
static int      PMI_debug = 0;
static int      PMI_spawned = 0;


/* Portals and networking state */
static ptl_handle_ni_t ni_h;
static ptl_process_t   my_id;
static ptl_handle_eq_t tx_eq_h;
static ptl_handle_eq_t rx_eq_h;
static ptl_pt_index_t  pt_index;
static ptl_md_t        tx_md;
static ptl_handle_md_t tx_md_h;
static ptl_me_t        rx_me;
static ptl_handle_me_t rx_me_h;
static char *request;
static char *response;


static int
init_portals(void)
{
	if ((request = malloc(PCT_MAX_PMI_MSG)) == NULL) {
		return -ENOMEM;
	}

	if ((response = malloc(PCT_MAX_PMI_MSG)) == NULL) {
		free(request);
		return -ENOMEM;
	}

	PTL_CHECK(PtlInit());
	PTL_CHECK(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING|PTL_NI_PHYSICAL, getpid(), NULL, NULL, &ni_h));
	PTL_CHECK(PtlGetPhysId(ni_h, &my_id));
	PTL_CHECK(PtlEQAlloc(ni_h, 16, &tx_eq_h));
	PTL_CHECK(PtlEQAlloc(ni_h, 16, &rx_eq_h));
	PTL_CHECK(PtlPTAlloc(ni_h, 0, rx_eq_h, PCT_PMI_PT_INDEX, &pt_index));

	/* Setup for the request transmit */
	tx_md.start        = request;
	tx_md.length       = PCT_MAX_PMI_MSG;
	tx_md.options      = 0;
	tx_md.eq_handle    = tx_eq_h;
	tx_md.ct_handle    = PTL_CT_NONE;

	/* Setup for the response receive */
	rx_me.start       = response;
	rx_me.length      = PCT_MAX_PMI_MSG;
	rx_me.ct_handle   = PTL_CT_NONE;
	rx_me.uid         = PCT_PTL_UID;
	rx_me.options     = PTL_ME_OP_PUT | PTL_ME_IS_ACCESSIBLE | PTL_ME_EVENT_LINK_DISABLE;
	rx_me.match_id    = (ptl_process_t){.phys={my_id.phys.nid, PCT_PTL_PID}},
	rx_me.match_bits  = 0;
	rx_me.ignore_bits = 0;
	rx_me.min_free    = 0;

	/* Tell Portals about the buffers */
	PTL_CHECK(PtlMDBind(ni_h, &tx_md, &tx_md_h));
	PTL_CHECK(PtlMEAppend(ni_h, pt_index, &rx_me, PTL_PRIORITY_LIST, NULL, &rx_me_h));

	return 0;
}


static int
fini_portals(void)
{
	PTL_CHECK(PtlMEUnlink(rx_me_h));
	PTL_CHECK(PtlMDRelease(tx_md_h));
	PTL_CHECK(PtlPTFree(ni_h, pt_index));
	PTL_CHECK(PtlEQFree(rx_eq_h));
	PTL_CHECK(PtlEQFree(tx_eq_h));
	free(request);
	free(response);
	return 0;
}


/* Sends the string in 'request' global variable to the PCT */
static int
send_to_pct(void)
{
	ptl_process_t target = (ptl_process_t){.phys={my_id.phys.nid, PCT_PTL_PID}};
	ptl_event_t ev;

	/* Send the Request */
	PTL_CHECK(
		ptl_enqueue(target, PCT_PMI_PT_INDEX, tx_md_h,
		            0, strlen(request) + 1, tx_eq_h)
	);

	/* Wait for the Response */
	PTL_CHECK(PtlEQWait(rx_eq_h, &ev));
	PTL_ASSERT(ev.type == PTL_EVENT_PUT);
	PTL_ASSERT(ev.ni_fail_type == PTL_NI_OK);
	PTL_ASSERT(ev.mlength == ev.rlength);

	return 0;
}


/* 
 * This function is used to request information from the server and check
 * that the response uses the expected command name.  On a successful
 * return from this routine, additional PMIU_getval calls may be used
 * to access information about the returned value.
 *
 * If checkRc is true, this routine also checks that the rc value returned
 * was 0.  If not, it uses the "msg" value to report on the reason for
 * the failure.
 */
static int
GetResponse(const char *req, const char *expectedCmd, int checkRc)
{
	int status, n;
	char *p;
	char recvbuf[PMIU_MAXLINE];
	char cmdName[PMIU_MAXLINE];

	/* This sends the request string in global var 'request' to PCT */
	strcpy(request, req);
	/* printf("REQUEST : %s", request); */
	send_to_pct();
	/* printf("RESPONSE: %s", response); */

	status = PMIU_parse_keyvals(response);
	if (status) {
		PMIU_printf(1, "parse_kevals failed %d\n", status);
		return status;
	}

	p = PMIU_getval("cmd", cmdName, sizeof(cmdName));
	if (!p) {
		PMIU_printf(1, "getval cmd failed\n");
		return PMI_FAIL;
	}

	if (strcmp(expectedCmd, cmdName) != 0) {
		PMIU_printf(1, "expecting cmd=%s, got %s\n", expectedCmd, cmdName);
		return PMI_FAIL;
	}

	if (checkRc) {
		p = PMIU_getval("rc", cmdName, PMIU_MAXLINE);
		if (p && strcmp(cmdName,"0") != 0) {
			PMIU_getval("msg", cmdName, PMIU_MAXLINE);
			PMIU_printf(1, "Command %s failed, reason='%s'\n",request,cmdName);
			return PMI_FAIL;
		}
	}

	return PMI_SUCCESS;
}


static int
PMII_getmaxes(int *kvsname_max, int *keylen_max, int *vallen_max)
{
	char cmd[PMIU_MAXLINE];
	int status, rc;

	/* Send request to the PCT */
	rc = snprintf(request, PCT_MAX_PMI_MSG,
	              "cmd=init pmi_version=%d pmi_subversion=%d task_id=%d\n",
	              PMI_VERSION, PMI_SUBVERSION, getpid());
	if (rc < 0) {
		fprintf(stderr, "failed to write cmd=init request\n");
		abort();
	}
	send_to_pct();

	/* Parse the PCT's response */
	PMIU_parse_keyvals(response);
	cmd[0] = 0;
	PMIU_getval("cmd", cmd, PMIU_MAXLINE);
	if (strncmp(cmd, "response_to_init", PMIU_MAXLINE) != 0) {
		fprintf(stderr, 
		        "got unexpected response to init :%s: (full line = %s)",
		        cmd, response);
		abort();
	} else {
		char buf[PMIU_MAXLINE];
		char buf1[PMIU_MAXLINE];
		PMIU_getval("rc", buf, PMIU_MAXLINE);
		if (strncmp(buf, "0", PMIU_MAXLINE) != 0) {
			PMIU_getval("pmi_version", buf, PMIU_MAXLINE);
			PMIU_getval("pmi_subversion", buf1, PMIU_MAXLINE);
			fprintf(stderr, 
			        "pmi_version mismatch; client=%d.%d mgr=%s.%s",
			        PMI_VERSION, PMI_SUBVERSION, buf, buf1);
			abort();
		}
	}

	status = GetResponse("cmd=get_maxes\n", "maxes", 0);
	if (status == PMI_SUCCESS) {
		char buf[PMIU_MAXLINE];
		PMIU_getval("kvsname_max", buf, PMIU_MAXLINE);
		*kvsname_max = atoi(buf);
		PMIU_getval("keylen_max", buf, PMIU_MAXLINE);
		*keylen_max = atoi(buf);
		PMIU_getval("vallen_max", buf, PMIU_MAXLINE);
		*vallen_max = atoi(buf);
	}

	return status;
}


int
PMI_Initialized(int *initialized)
{
	/* Turn this into a logical value (1 or 0). This allows us
	 * to use PMI_initialized to distinguish between initialized with
	 * an PMI service (e.g., via mpiexec) and the singleton init, 
	 * which has no PMI service. */
	*initialized = (PMI_initialized != 0);
	return PMI_SUCCESS;
}


int
PMI_Init(int *spawned)
{
	char *p;
	int i;

	PMI_initialized = PMI_UNINITIALIZED;

	if ((p = getenv("PMI_DEBUG")) != NULL) {
		PMI_debug = atoi(p);
	} else {
		PMI_debug = 0;
	}

	if ((p = getenv("PMI_SIZE")) != NULL) {
		PMI_size = atoi(p);
	} else {
		return PMI_FAIL;
	}
	
	if ((p = getenv("PMI_RANK")) != NULL) {
		PMI_rank = atoi(p);
		/* Let the util routine know the rank of this process for 
		   any messages (usually debugging or error) */
		PMIU_Set_rank(PMI_rank);
	} else {
		return PMI_FAIL;
	}

	if (init_portals() != 0)
		return PMI_FAIL;

	if (PMII_getmaxes(&PMI_kvsname_max, &PMI_keylen_max, &PMI_vallen_max) != 0)
		return PMI_FAIL;

	*spawned = 0;

	PMI_initialized = NORMAL_INIT_WITH_PM;

	return PMI_SUCCESS;
}


int
PMI_Get_size(int *size)
{
	*size = PMI_size;
	return PMI_SUCCESS;
}


int
PMI_Get_rank(int *rank)
{
	*rank = PMI_rank;
	return PMI_SUCCESS;
}


int
PMI_Get_universe_size(int *size)
{
	int  status;
	char buf[PMIU_MAXLINE];

	if ((status = GetResponse("cmd=get_universe_size\n", "universe_size", 0)) != PMI_SUCCESS)
		return status;

	PMIU_getval("size", buf, PMIU_MAXLINE);
	*size = atoi(buf);

	return PMI_SUCCESS;
}


int 
PMI_Get_appnum(int *appnum)
{
	int  status;
	char buf[PMIU_MAXLINE];

	if ((status = GetResponse("cmd=get_appnum\n", "appnum", 0)) != PMI_SUCCESS)
		return status;

	PMIU_getval("appnum", buf, PMIU_MAXLINE);
	*appnum = atoi(buf);

	return PMI_SUCCESS;
}


int
PMI_Barrier(void)
{
	return GetResponse("cmd=barrier_in\n", "barrier_out", 0);
}


int
PMI_Finalize(void)
{
	int status = GetResponse("cmd=finalize\n", "finalize_ack", 0);
	fini_portals();
	return status;
}


int
PMI_Abort(int exit_code, const char *error_msg)
{
	PMIU_printf(1, "aborting job:\n%s\n", error_msg);
	exit(exit_code);
	return -1;
}


/************************************* Keymap functions **********************/


int
PMI_KVS_Get_my_name(char *kvsname, int length)
{
	int status;

	if ((status = GetResponse("cmd=get_my_kvsname\n", "my_kvsname", 0)) != PMI_SUCCESS)
		return status;

	PMIU_getval("kvsname", kvsname, length);

	return PMI_SUCCESS;
}


int
PMI_KVS_Get_name_length_max(int *maxlen)
{
	*maxlen = PMI_kvsname_max;
	return PMI_SUCCESS;
}


int
PMI_KVS_Get_key_length_max(int *maxlen)
{
	*maxlen = PMI_keylen_max;
	return PMI_SUCCESS;
}


int
PMI_KVS_Get_value_length_max(int *maxlen)
{
	*maxlen = PMI_vallen_max;
	return PMI_SUCCESS;
}


int
PMI_KVS_Put(const char *kvsname, const char *key, const char *value)
{
	char buf[PMIU_MAXLINE];
	int status;

	status = snprintf(buf, PMIU_MAXLINE, 
	                  "cmd=put kvsname=%s key=%s value=%s\n",
	                  kvsname, key, value);
	if (status < 0)
		return PMI_FAIL;

	return GetResponse(buf, "put_result", 1);
}


int
PMI_KVS_Get(const char *kvsname, const char *key, char *value, int length)
{
	char buf[PMIU_MAXLINE];
	int status, rc;

	status = snprintf(buf, PMIU_MAXLINE,
	                  "cmd=get kvsname=%s key=%s\n",
	                  kvsname, key);
	if (status < 0)
		return PMI_FAIL;

	status = GetResponse(buf, "get_result", 0);
	if (status != PMI_SUCCESS)
		return status;

	PMIU_getval("rc", buf, PMIU_MAXLINE);
	if ((rc = atoi(buf)) != 0)
		return PMI_FAIL;

	PMIU_getval("value", value, length);

	return PMI_SUCCESS;
}


int
PMI_KVS_Commit(const char *kvsname)
{
	/* Commits push our local kvs to the server */
	char buf[PMIU_MAXLINE];
	int status, rc;

	status = snprintf(buf, PMIU_MAXLINE,
					  "cmd=commit kvsname=%s\n",
					  kvsname);
	
	if (status < 0)
        return PMI_FAIL;

	status = GetResponse(buf, "commit_result", 0);
	if (status != PMI_SUCCESS)
		return status;

	return PMI_SUCCESS;
}


/*************************** Name Publishing functions **********************/


int
PMI_Publish_name(const char *service_name, const char *port)
{
	char buf[PMIU_MAXLINE];
	int status;

	status = snprintf(buf, PMIU_MAXLINE,
	                  "cmd=publish_name service=%s port=%s\n",
	                  service_name, port);
	if (status < 0)
		return PMI_FAIL;

	status = GetResponse(buf, "publish_result", 0);
	if (status != PMI_SUCCESS)
		return status;

	/* FIXME: This should have used rc and msg */
	PMIU_getval("info", buf, PMIU_MAXLINE);
	if (strcmp(buf, "ok") != 0) {
		PMIU_printf(1, "publish failed; reason = %s\n", buf);
		return PMI_FAIL;
	}

	return PMI_SUCCESS;
}


int
PMI_Unpublish_name(const char *service_name)
{
	char buf[PMIU_MAXLINE];
	int status;

	status = snprintf(buf, PMIU_MAXLINE,
	                  "cmd=unpublish_name service=%s\n", 
	                  service_name);
	if (status < 0)
		return PMI_FAIL;

	status = GetResponse(buf, "unpublish_result", 0);
	if (status != PMI_SUCCESS)
		return status;

	PMIU_getval("info", buf, PMIU_MAXLINE);
	if (strcmp(buf, "ok") != 0) {
		PMIU_printf(1, "unpublish failed; reason = %s\n", buf);
		return PMI_FAIL;
	}

	return PMI_SUCCESS;
}


int
PMI_Lookup_name(const char *service_name, char *port)
{
	char buf[PMIU_MAXLINE];
	int status;

	status = snprintf(buf, PMIU_MAXLINE,
	                  "cmd=lookup_name service=%s\n",
	                  service_name);
	if (status < 0)
		return PMI_FAIL;


	status = GetResponse(buf, "lookup_result", 0);
	if (status != PMI_SUCCESS)
		return status;

	PMIU_getval("info", buf, PMIU_MAXLINE);
	if (strcmp(buf, "ok") != 0) {
		PMIU_printf(1, "lookup failed; reason = %s\n", buf);
		return PMI_FAIL;
	}

	PMIU_getval("port", port, MPI_MAX_PORT_NAME);

	return PMI_SUCCESS;
}


/************************** Process Creation functions **********************/


int
PMI_Spawn_multiple(int			count,
                   const char *         cmds[],
                   const char **        argvs[],
                   const int            maxprocs[],
                   const int            info_keyval_sizes[],
                   const PMI_keyval_t * info_keyval_vectors[],
                   int                  preput_keyval_size,
                   const PMI_keyval_t   preput_keyval_vector[],
                   int                  errors[]
)
{
	return PMI_FAIL;
}

/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef HYDRA_H_INCLUDED
#define HYDRA_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <portals4.h>
#include <portals4_util.h>

extern char *HYD_dbg_prefix;

#define HYD_TMPBUF_SIZE     (64 * 1024)
#define HYD_TMP_STRLEN      1024
#define HYD_NUM_TMP_STRINGS 1000

/* Status information */
typedef enum {
    HYD_SUCCESS = 0,
    HYD_FAILURE,        /* general failure */

    /* Silent errors */
    HYD_GRACEFUL_ABORT,
    HYD_TIMED_OUT,

    /* Regular errors */
    HYD_NO_MEM,
    HYD_SOCK_ERROR,
    HYD_INVALID_PARAM,
    HYD_INTERNAL_ERROR
} HYD_status;

typedef unsigned short HYD_event_t;

/* Argument matching functions */
struct HYD_arg_match_table {
    const char *arg;
    HYD_status(*handler_fn) (char *arg, char ***argv_p);
    void (*help_fn) (void);
};

struct HYD_env_global {
    struct HYD_env *system;
    struct HYD_env *user;
    struct HYD_env *inherited;
    char *prop;
};

/* Executable information */
struct HYD_exec {
    char *exec[HYD_NUM_TMP_STRINGS];
    char *wdir;

    int proc_count;
    struct HYD_env *user_env;
    char *env_prop;

    int appnum;

    struct HYD_exec *next;
};

/* Global user parameters */
struct HYD_user_global {
    /* RMK */
    char *rmk;

    /* Launcher */
    char *launcher;
    char *launcher_exec;

    /* Processor topology */
    char *binding;
    char *topolib;

    /* Checkpoint restart */
    char *ckpointlib;
    char *ckpoint_prefix;
    int ckpoint_num;

    /* Demux engine */
    char *demux;

    /* Network interface */
    char *iface;

    /* Other random parameters */
    int enablex;
    int debug;

    int auto_cleanup;

    struct HYD_env_global global_env;
};

/* Disable for now; we might add something here in the future */
#define HYDU_FUNC_ENTER()   do {} while (0)
#define HYDU_FUNC_EXIT()    do {} while (0)

#define HYDU_dump_prefix(fp)                                            \
    {                                                                   \
        fprintf(fp, "[%s] ", HYD_dbg_prefix);                           \
        fflush(fp);                                                     \
    }

#define HYDU_dump_noprefix(fp, ...)                                     \
    {                                                                   \
        fprintf(fp, __VA_ARGS__);                                       \
        fflush(fp);                                                     \
    }

#define HYDU_dump(fp, ...)                                              \
    {                                                                   \
        HYDU_dump_prefix(fp);                                           \
        HYDU_dump_noprefix(fp, __VA_ARGS__);                            \
    }

#define HYDU_error_printf(...)                                          \
    {                                                                   \
        HYDU_dump_prefix(stderr);                                       \
        HYDU_dump_noprefix(stderr, "%s (%d): ", __FILE__, __LINE__);    \
        HYDU_dump_noprefix(stderr, __VA_ARGS__);                        \
    }

#define HYD_SILENT_ERROR(status) \
	(((status) == HYD_GRACEFUL_ABORT) || ((status) == HYD_TIMED_OUT))

#define HYDU_ASSERT(x, status)                                          \
    {                                                                   \
        if (!(x)) {                                                     \
            HYDU_ERR_SETANDJUMP(status, HYD_INTERNAL_ERROR,             \
                                 "assert (%s) failed\n", #x);           \
        }                                                               \
    }

#define HYDU_IGNORE_TIMEOUT(status)                                     \
    {                                                                   \
        if ((status) == HYD_TIMED_OUT)                                  \
            (status) = HYD_SUCCESS;                                     \
    }

#define HYDU_ERR_POP(status, ...)                                       \
    {                                                                   \
        if (status && !HYD_SILENT_ERROR(status)) {                      \
            HYDU_error_printf(__VA_ARGS__);                             \
            goto fn_fail;                                               \
        }                                                               \
        else if (HYD_SILENT_ERROR(status)) {                            \
            goto fn_exit;                                               \
        }                                                               \
    }

#define HYDU_ERR_SETANDJUMP(status, error, ...)                         \
    {                                                                   \
        status = error;                                                 \
        HYDU_ERR_POP(status, __VA_ARGS__);                              \
    }

#define HYDU_ERR_CHKANDJUMP(status, chk, error, ...)                    \
    {                                                                   \
        if ((chk))                                                      \
            HYDU_ERR_SETANDJUMP(status, error, __VA_ARGS__);            \
    }

#define HYDU_MALLOC(p, type, size, status)                              \
    {                                                                   \
        HYDU_ASSERT(size, status);                                      \
        (p) = (type) HYDU_malloc((size));                               \
        if ((p) == NULL)                                                \
            HYDU_ERR_SETANDJUMP((status), HYD_NO_MEM,                   \
                                "failed to allocate %d bytes\n",        \
                                (int) (size));                          \
    }

#define HYDU_FREE(p)                                                    \
    {                                                                   \
        HYDU_free((void *) p);                                          \
    }

#define HYDU_mem_init()
#define HYDU_strdup   strdup
#define HYDU_strtok   strtok
#define HYDU_strcat   strcat
#define HYDU_strncpy  strncpy
#define HYDU_malloc   malloc
#define HYDU_free     free
#define HYDU_snprintf snprintf

HYD_status HYDU_list_append_strlist(char **exec, char **client_arg);
HYD_status HYDU_print_strlist(char **args);
void HYDU_free_strlist(char **args);
HYD_status HYDU_str_alloc_and_join(char **strlist, char **strjoin);
HYD_status HYDU_strsplit(char *str, char **str1, char **str2, char sep);
HYD_status HYDU_strdup_list(char *src[], char **dest[]);
char *HYDU_size_t_to_str(size_t x);
char *HYDU_int_to_str(int x);
char *HYDU_int_to_str_pad(int x, int maxlen);
int HYDU_strlist_lastidx(char **strlist);
char **HYDU_str_to_strlist(char *str);

#endif

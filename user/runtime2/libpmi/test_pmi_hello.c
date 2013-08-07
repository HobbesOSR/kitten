#define _GNU_SOURCE
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <pmi.h>

int
main(int argc,
     char *argv[])
{
    int initialized, rank, size, usize;
    int i, max_name_len, max_key_len, max_val_len;
    char *name, *key, *val;

    if (PMI_SUCCESS != PMI_Initialized(&initialized)) {
        return 1;
    }

    if (0 == initialized) {
        if (PMI_SUCCESS != PMI_Init(&initialized)) {
            return 1;
        }
    }

    if (PMI_SUCCESS != PMI_Get_rank(&rank)) {
        return 1;
    }

    if (PMI_SUCCESS != PMI_Get_size(&size)) {
        return 1;
    }

	if (PMI_SUCCESS != PMI_Get_universe_size(&usize)) {
		return 1;
	}

    printf("Hello, World. I am rank %d on cpu %d, world size=%d, universe size=%d.\n",
	       rank, sched_getcpu(), size, usize);

    if (PMI_SUCCESS != PMI_KVS_Get_name_length_max(&max_name_len)) {
        return 1;
    }
    name = (char*) malloc(max_name_len);
    if (NULL == name) return 1;

    if (PMI_SUCCESS != PMI_KVS_Get_key_length_max(&max_key_len)) {
        return 1;
    }
    key = (char*) malloc(max_key_len);
    if (NULL == key) return 1;

    if (PMI_SUCCESS != PMI_KVS_Get_value_length_max(&max_val_len)) {
        return 1;
    }
    val = (char*) malloc(max_val_len);
    if (NULL == val) return 1;

    if (PMI_SUCCESS != PMI_KVS_Get_my_name(name, max_name_len)) {
        return 1;
    }

    /* put my information */
    snprintf(key, max_key_len, "pmi_hello-%lu-test", (long unsigned) rank);
    snprintf(val, max_val_len, "%lu", (long unsigned) rank);

    if (PMI_SUCCESS != PMI_KVS_Put(name, key, val)) {
        return 1;
    }

    if (PMI_SUCCESS != PMI_KVS_Commit(name)) {
        return 1;
    }

    if (PMI_SUCCESS != PMI_Barrier()) {
        return 1;
    }

    /* verify everyone's information */
    for (i = 0 ; i < size ; ++i) {
        snprintf(key, max_key_len, "pmi_hello-%lu-test", (long unsigned) i);

        if (PMI_SUCCESS != PMI_KVS_Get(name, key, val, max_val_len)) {
            return 1;
        }

        if (i != strtol(val, NULL, 0)) {
            fprintf(stderr, "%d: Error: Expected %d, got %d\n", rank, i, (int) strtol(val, NULL, 0));
            return 1;
        }
    }

    PMI_Finalize();

	printf("SUCCESS\n");

    return 0;
}

/* vim:set expandtab: */

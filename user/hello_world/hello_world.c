/* Copyright (c) 2008, Sandia National Laboratories */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <lwk/liblwk.h>

static void pmem_api_test(void);
static void aspace_api_test(void);

int
main(int argc, char *argv[], char *envp[])
{
	int i;
	id_t aspace_id;

	printf("Hello, world!\n");

	printf("Arguments:\n");
	for (i = 0; i < argc; i++)
		printf("  argv[%d] = %s\n", i, argv[i]);

	printf("Environment Variables:\n");
	for (i = 0; envp[i] != NULL; i++)
		printf("  envp[%d] = %s\n", i, envp[i]);

	pmem_api_test();
	aspace_api_test();

	printf("Spinning forever...\n");
	while (1) {}
}

static void
pmem_api_test(void)
{
	struct pmem_region query, result;
	unsigned long bytes_umem = 0;
	int status;

	printf("TEST BEGIN: Physical Memory Management\n");

	query.start = 0;
	query.end = ULONG_MAX;
	pmem_region_unset_all(&query);

	printf("  Physical Memory Map:\n");
	while ((status = pmem_query(&query, &result)) == 0) {
		printf("    [%#016lx, %#016lx) %-11s\n",
			result.start,
			result.end,
			(result.type_is_set)
				? pmem_type_to_string(result.type)
				: "UNSET"
		);

		if (result.type == PMEM_TYPE_UMEM)
			bytes_umem += (result.end - result.start);

		query.start = result.end;
	}

	if (status != -ENOENT) {
		printf("ERROR: pmem_query() status=%d\n", status);
	}

	printf("  Total User-Level Managed Memory: %lu bytes\n", bytes_umem);

	printf("TEST END: Physical Memory Management\n");
}

static void
aspace_api_test(void)
{
	int status;
	id_t my_id, new_id;

	printf("TEST BEGIN: Address Space Management\n");

	if ((status = aspace_get_myid(&my_id)) != 0)
		printf("ERROR: aspace_get_myid() status=%d\n", status);
	else
		printf("  My address space ID is %u\n", my_id);

	printf("  Creating a new aspace: ");

	status = aspace_create(ANY_ID, "TEST-ASPACE", &new_id);
	if (status)
		printf("\nERROR: aspace_create() status=%d\n", status);
	else
		printf("id=%u\n", new_id);

	printf("  Using SMARTMAP to map myself into aspace %u\n", new_id);
	status = aspace_smartmap(my_id, new_id, SMARTMAP_ALIGN, SMARTMAP_ALIGN);
	if (status) printf("ERROR: aspace_smartmap() status=%d\n", status);

	aspace_dump2console(new_id);

	status = aspace_unsmartmap(my_id, new_id);
	if (status) printf("ERROR: aspace_unsmartmap() status=%d\n", status);

	printf("  Destroying a aspace %u: ", new_id);
	status = aspace_destroy(new_id);
	if (status)
		printf("ERROR: aspace_destroy() status=%d\n", status);
	else
		printf("OK\n");

	printf("TEST END: Address Space Management\n");
}

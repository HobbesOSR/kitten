/* Copyright (c) 2008, Sandia National Laboratories */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <lwk/liblwk.h>

static void print_physical_memory_map(void);

int
main(int argc, char *argv[], char *envp[])
{
	int i;

	printf("Hello, world!\n");

	printf("Arguments:\n");
	for (i = 0; i < argc; i++)
		printf("  argv[%d] = %s\n", i, argv[i]);

	printf("Environment Variables:\n");
	for (i = 0; envp[i] != NULL; i++)
		printf("  envp[%d] = %s\n", i, envp[i]);

	printf("Physical Memory Map:\n");
	print_physical_memory_map();

	printf("Spinning forever...\n");
	while (1) {}
}

static void
print_physical_memory_map(void)
{
	struct pmem_region query, result;
	unsigned long bytes_umem = 0;
	int status;

	query.start = 0;
	query.end = ULONG_MAX;
	pmem_region_unset_all(&query);

	while ((status = pmem_query(&query, &result)) == 0) {
		printf("  [%#016lx, %#016lx) %-11s\n",
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

	printf("Total User-Level Managed Memory: %lu bytes\n", bytes_umem);
}

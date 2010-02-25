/* Copyright (c) 2008, Sandia National Laboratories */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sched.h>
#include <pthread.h>
#include <lwk/liblwk.h>
#include <sys/types.h>
#include <sys/time.h>

static int pmem_api_test(void);
static int aspace_api_test(void);
static int task_api_test(void);
static int fd_test(void);
static int hypervisor_api_test(void);

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

	pmem_api_test();
	aspace_api_test();
	fd_test();
	task_api_test();
	hypervisor_api_test();

	printf("Spinning forever...\n");
	for (i = 0; i < 10; i++) {
		sleep(5);
		printf("%s: Meow %d!\n", __func__, i );
	}
	printf("   That's all, folks!\n");

	while(1)
		sleep(100000);
}

static int
pmem_api_test(void)
{
	struct pmem_region query, result;
	unsigned long bytes_umem = 0;
	int status;

	printf("\nTEST BEGIN: Physical Memory Management\n");

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
		return -1;
	}

	printf("  Total User-Level Managed Memory: %lu bytes\n", bytes_umem);

	printf("TEST END: Physical Memory Management\n");
	return 0;
}

static int
aspace_api_test(void)
{
	int status;
	id_t my_id, new_id;

	printf("\nTEST BEGIN: Address Space Management\n");

	status = aspace_get_myid(&my_id);
	if (status) {
		printf("ERROR: aspace_get_myid() status=%d\n", status);
		return -1;
	}
	printf("  My address space ID is %u\n", my_id);

	printf("  Creating a new aspace: ");
	status = aspace_create(ANY_ID, "TEST-ASPACE", &new_id);
	if (status) {
		printf("\nERROR: aspace_create() status=%d\n", status);
		return -1;
	}
	printf("id=%u\n", new_id);

	printf("  Using SMARTMAP to map myself into aspace %u\n", new_id);
	status = aspace_smartmap(my_id, new_id, SMARTMAP_ALIGN, SMARTMAP_ALIGN);
	if (status) {
		printf("ERROR: aspace_smartmap() status=%d\n", status);
		return -1;
	}

	aspace_dump2console(new_id);

	status = aspace_unsmartmap(my_id, new_id);
	if (status) {
		printf("ERROR: aspace_unsmartmap() status=%d\n", status);
		return -1;
	}

	printf("  Destroying a aspace %u: ", new_id);
	status = aspace_destroy(new_id);
	if (status) {
		printf("ERROR: aspace_destroy() status=%d\n", status);
		return -1;
	}
	printf("OK\n");

	printf("TEST END: Address Space Management\n");
	return 0;
}

static void *
hello_world_thread(void *arg)
{
	int i;
	const unsigned long id = (unsigned long) arg;
	id_t my_cpu;
	struct timeval tv;

	/* Pause a bit to avoid cluttering up the console */
	sleep(1);

	task_get_cpu(&my_cpu);

	printf( "%ld: Hello from a thread on cpu %u\n", id, my_cpu );
	for (i = 0; i < 10; i++) {
		sleep(5);
		gettimeofday(&tv, NULL);
		printf( "%ld: Meow %d! now=%ld.%06ld\n",
			id, i, tv.tv_sec, tv.tv_usec );
	}
	printf( "%ld: That's all, folks!\n", id );

	while(1)
		sleep(100000);
}

static int 
task_api_test(void)
{
	int status;
	unsigned i;
	id_t my_id, my_cpu;
	user_cpumask_t my_cpumask;
	pthread_t tid;

	printf("\nTEST BEGIN: Task Management\n");

	status = task_get_myid(&my_id);
	if (status) {
		printf("ERROR: task_get_myid() status=%d\n", status);
		return -1;
	}

	status = task_get_cpu(&my_cpu);
	if (status) {
		printf("ERROR: task_get_cpu() status=%d\n", status);
		return -1;
	}

	status = task_get_cpumask(&my_cpumask);
	if (status) {
		printf("ERROR: task_get_cpumask() status=%d\n", status);
		return -1;
	}

	printf("  My task ID is %u\n", my_id);
	printf("  I'm executing on CPU %u\n", my_cpu);
	printf("  The following CPUs are in my cpumask:\n    ");
	for (i = CPU_MIN_ID; i <= CPU_MAX_ID; i++) {
		if (cpu_isset(i, my_cpumask))
			printf("%d ", i);
	}
	printf("\n");

	printf("  Creating a thread on each CPU:\n");
	for (i = CPU_MIN_ID; i <= CPU_MAX_ID; i++) {
		if (!cpu_isset(i, my_cpumask))
			continue;

		int rc = pthread_create(
			&tid,
			NULL,
			&hello_world_thread,
			(void*) (uintptr_t) i
		);

		printf( "    thread %d: rc=%d\n", i, rc );
	}

	printf("TEST END: Task Management\n");
	return 0;
}

/* writen() is from "UNIX Network Programming" by W. Richard Stevents */
static ssize_t 
writen(int fd, const void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write(fd, ptr, nleft)) <= 0) {
			if (errno == EINTR) {
				nwritten = 0;  /* and call write() again */
			} else {
				perror( "write" );
				return -1;     /* error */
			}
		}
		nleft -= nwritten;
		ptr   += nwritten;
	}
	return n;
}


int
fd_test( void )
{
	char buf[ 1024 ] = "";
	int fd;
	ssize_t rc;

	printf( "Testing open\n" );
	fd = open( "/sys", O_RDONLY );
	rc = read( fd, buf, sizeof(buf) );
	if( rc >= 0 )
		printf( "fd %d rc=%ld '%s'\n", fd, rc, buf );
	close( fd );

	fd = open( "/sys/kernel/dummy/int", O_RDONLY );
	rc = read( fd, buf, sizeof(buf) );
	if( rc >= 0 )
		printf( "fd %d rc=%ld '%s'\n", fd, rc, buf );
	close( fd );

	fd = open( "/sys/kernel/dummy/hex", O_RDONLY );
	rc = read( fd, buf, sizeof(buf) );
	if( rc >= 0 )
		printf( "fd %d rc=%ld '%s'\n", fd, rc, buf );
	close( fd );

	fd = open( "/sys/kernel/dummy/bin", O_RDONLY );
	uint64_t val;
	rc = read( fd, &val, sizeof(val) );
	if( rc >= 0 )
		printf( "fd %d rc=%ld '%s'\n", fd, rc, buf );
	close( fd );

	// Should fail
	fd = open( "/sys/kernel/dummy", O_RDONLY );
	rc = read( fd, buf, sizeof(buf) );
	if( rc >= 0 )
		printf( "fd %d rc=%ld '%s'\n", fd, rc, buf );
	close( fd );

	return 0;
}

/* These specify the virtual address start, end, and size
 * of the guest OS ISO image embedded in our ELF executable. */
int _binary_hello_world_rawdata_size  __attribute__ ((weak));
int _binary_hello_world_rawdata_start __attribute__ ((weak));
int _binary_hello_world_rawdata_end   __attribute__ ((weak));

static int
hypervisor_api_test(void)
{
	volatile size_t  iso_size  = (size_t)  &_binary_hello_world_rawdata_size;
	volatile vaddr_t iso_start = (vaddr_t) &_binary_hello_world_rawdata_start;
	volatile vaddr_t iso_end   = (vaddr_t) &_binary_hello_world_rawdata_end;
	paddr_t iso_start_paddr;
	id_t my_aspace;
	int status;

	printf("\nTEST BEGIN: Hypervisor API\n");

	printf("  Starting a guest OS...\n");

	/* Make sure there is an embedded ISO image */
	if (iso_size != (iso_end - iso_start)) {
		printf("    Failed, no ISO image available.\n");
		return -1;
	}

	/* Determine the physical address of the ISO image */
	aspace_get_myid(&my_aspace);
	aspace_virt_to_phys(my_aspace, iso_start, &iso_start_paddr);

	/* Fire it up! */
	status = v3_start_guest(iso_start_paddr, iso_size);
	if (status) {
		printf("    Failed (status=%d).\n", status);
		return -1;
	}

	printf("    Success.\n");

	printf("TEST END: Hypervisor API\n");
	return 0;
}

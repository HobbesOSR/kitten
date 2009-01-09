/* Copyright (c) 2008, Sandia National Laboratories */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <sched.h>
#include <pthread.h>
#include <lwk/liblwk.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

static int pmem_api_test(void);
static int aspace_api_test(void);
static int task_api_test(void);
static int socket_api_test(void);

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
	task_api_test();
	socket_api_test();

	printf("Spinning forever...\n");
	for (i = 0; i < 10; i++) {
		sleep(5);
		printf("%s: Meow %d!\n", __func__, i );
	}
	printf("   That's all, folks!\n");

	while(1)
		sched_yield();
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

	/* Pause a bit to avoid cluttering up the console */
	sleep(1);

	task_get_cpu(&my_cpu);

	printf( "%ld: Hello from a thread on cpu %u\n", id, my_cpu );
	for (i = 0; i < 10; i++) {
		sleep(5);
		printf( "%ld: Meow %d!\n", id, i );
	}
	printf( "%ld: That's all, folks!\n", id );

	while(1)
		sched_yield();
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
socket_api_test( void )
{
	printf("\nTEST BEGIN: Sockets API\n");

	int port = 80;
	int s = socket( PF_INET, SOCK_STREAM, 0 );
	if( s < 0 )
	{
		printf( "ERROR: socket() failed! rc=%d\n", s );
		return -1;
	}

	struct sockaddr_in addr, client;
	addr.sin_family = AF_INET;
	addr.sin_port = htons( port );
	addr.sin_addr.s_addr = INADDR_ANY;
	if( bind( s, (struct sockaddr*) &addr, sizeof(addr) ) < 0 )
		perror( "bind" );

	if( listen( s, 5 ) < 0 ) {
		perror( "listen" );
		return -1;
	}

	int max_fd = s;
	fd_set fds;
	FD_ZERO( &fds );
	FD_SET( s, &fds );

	printf( "Going into mainloop: server fd %d on port %d\n", s, port );

	while(1)
	{
		fd_set read_fds = fds;
		int rc = select( max_fd+1, &read_fds, 0, 0, 0 );
		if( rc < 0 )
		{
			printf( "ERROR: select() failed! rc=%d\n", rc );
			return -1;
		}

		if( FD_ISSET( s, &read_fds ) )
		{
			printf( "new connection\n" );
			socklen_t socklen = sizeof(client);
			int new_fd = accept(
				s,
				(struct sockaddr *) &client,
				&socklen
			);

			printf( "connected newfd %d!\n", new_fd );
			writen( new_fd, "Welcome!\n", 9 );
			FD_SET( new_fd, &fds );
			if( new_fd > max_fd )
				max_fd = new_fd;
		}

		int fd;
		for( fd=0 ; fd<=max_fd ; fd++ )
		{
			if( fd == s || !FD_ISSET( fd, &read_fds ) )
				continue;

			char buf[ 33 ];
			ssize_t rc = read( fd, buf, sizeof(buf)-1 );
			if( rc <= 0 )
			{
				printf( "%d: closed connection rc=%zd\n", fd, rc );
				FD_CLR( fd, &fds );
				continue;
			}

			if( rc == 0 )
				continue;

			buf[rc] = '\0';

			// Trim any newlines off the end
			while( rc > 0 )
			{
				if( !isspace( buf[rc-1] ) )
					break;
				buf[--rc] = '\0';
			}

			printf( "%d: read %zd bytes: '%s'\n",
				fd,
				rc,
				buf
			);

			char outbuf[ 32 ];
			int len = snprintf( outbuf, sizeof(outbuf), "%d: read %zd bytes\n", fd, rc );
			writen( fd, outbuf, len );
		}
	}

	printf("TEST END: Sockets API\n");
	return 0;
}

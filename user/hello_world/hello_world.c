/* Copyright (c) 2008, Sandia National Laboratories */

#define _GNU_SOURCE

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
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <utmpx.h>
#include <unistd.h>
#include <sys/syscall.h>

static int pmem_api_test(void);
static int aspace_api_test(void);
static int task_api_test(void);
static int task_migrate_test(void);
static int fd_test(void);
static int socket_api_test(void);
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
	task_migrate_test();
	hypervisor_api_test();
        socket_api_test();

	printf("\n");
	printf("ALL TESTS COMPLETE\n");

	printf("\n");
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

	printf("\n");
	printf("TEST BEGIN: Physical Memory Management\n");

	query.start = 0;
	query.end = ULONG_MAX;
	pmem_region_unset_all(&query);

	printf("  Physical Memory Map:\n");
	while ((status = pmem_query(&query, &result)) == 0) {
		printf("    [%#016lx, %#016lx) %-10s numa_node=%u\n",
			result.start,
			result.end,
			(result.type_is_set)
				? pmem_type_to_string(result.type)
				: "UNSET",
			(result.numa_node_is_set)
				? result.numa_node
				: 0
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

	printf("TEST END:   Physical Memory Management\n");
	return 0;
}

static int
aspace_api_test(void)
{
	int status;
	id_t my_id, new_id;

	printf("\n");
	printf("TEST BEGIN: Address Space Management\n");

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

	printf("  Unmapping myself from aspace %u\n", new_id);
	status = aspace_unsmartmap(my_id, new_id);
	if (status) {
		printf("ERROR: aspace_unsmartmap() status=%d\n", status);
		return -1;
	}

	aspace_dump2console(new_id);

	printf("  Destroying a aspace %u: ", new_id);
	status = aspace_destroy(new_id);
	if (status) {
		printf("ERROR: aspace_destroy() status=%d\n", status);
		return -1;
	}
	printf("OK\n");

	printf("  Using SMARTMAP to do a self-mapping\n");
	status = aspace_smartmap(my_id, my_id, SMARTMAP_ALIGN, SMARTMAP_ALIGN);
	if (status) {
		printf("ERROR: aspace_smartmap() status=%d\n", status);
		return -1;
	}

	aspace_dump2console(my_id);

	printf("  Unmapping my self-mapping\n");
	status = aspace_unsmartmap(my_id, my_id);
	if (status) {
		printf("ERROR: aspace_unsmartmap() status=%d\n", status);
		return -1;
	}

	aspace_dump2console(my_id);

	printf("TEST END:   Address Space Management\n");
	return 0;
}


// Glibc doesn't provide a wrapper the Linux-specific gettid() call.
static pid_t
gettid(void)
{
	return syscall(__NR_gettid);
}


// This stores how many hello_world_thread() pthreads were
// actually created by the task_api_test() (below)
int num_threads = 0;
pthread_mutex_t num_threads_mutex = PTHREAD_MUTEX_INITIALIZER;


// Simple barrier implementation
int barrier_count = 0;
pthread_mutex_t barrier_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  barrier_cond  = PTHREAD_COND_INITIALIZER;
void
barrier(void)
{
	pthread_mutex_lock(&barrier_mutex);

	// All threads, including the main() thread increment this
	barrier_count++;

	// First num_threads-1 go to sleep, last thread wakes everyone up
	if (barrier_count != num_threads) {
		pthread_cond_wait(&barrier_cond, &barrier_mutex);
	} else {
		printf("All threads barrier\n");
		barrier_count = 0;
		pthread_cond_broadcast(&barrier_cond);
	}

	pthread_mutex_unlock(&barrier_mutex);
}


// The glibc in RHEL5 does not support sched_getcpu() even though
// the RHEL5 kernel does. To workaround, we implement our own
// sched_getcpu() in sched_getcpu.c and provide this prototype.
int sched_getcpu(void);


static void *
hello_world_thread(void *arg)
{
	int my_id = (int)(uintptr_t) arg;

	// Wait for num_threads to be set by task_api_test()
	pthread_mutex_lock(&num_threads_mutex);
	pthread_mutex_unlock(&num_threads_mutex);

	printf("Hello from thread %2d: pid=%2d tid=%2d cpu=%2d #threads=%d\n",
		my_id, getpid(), gettid(), sched_getcpu(), num_threads);

	barrier();

	printf("Goodbye from thread %d\n", my_id);

	barrier();

	pthread_exit((void *)(uintptr_t)my_id);
}


static int 
task_api_test(void)
{
	pthread_t pthread_id[CPU_SETSIZE];
	cpu_set_t cpu_mask;
	int status, i;
	void *value_ptr;
	pthread_attr_t attr;

	printf("\n");
	printf("TEST BEGIN: Task Management\n");

	// Set stack size to something small to avoid running out of heap
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 32);

	printf("  My Linux process id (PID) is: %d\n",  getpid());
	printf("  My Linux thread id  (TID) is: %d\n",  gettid());
	
	status = pthread_getaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
	if (status) {
		printf("    ERROR: pthread_getaffinity_np() status=%d\n", status);
		return -1;
	}

	printf("  My CPU mask is:               ");
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &cpu_mask))
			printf("%d ", i);
	}
	printf("\n");

	printf("  Creating one thread for each CPU:\n");
	pthread_mutex_lock(&num_threads_mutex);
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (!CPU_ISSET(i, &cpu_mask))
			continue;

		status = pthread_create(
			&pthread_id[num_threads],
			&attr,
			&hello_world_thread,
			(void *)(uintptr_t)num_threads+1
		);

		if (status) {
			printf("    ERROR: pthread_create() status=%d\n", status);
			continue;
		} else {
			printf("    created thread %d\n", num_threads+1);
			++num_threads;
		}
	}
	pthread_mutex_unlock(&num_threads_mutex);

	printf("  Waiting for threads to exit:\n");
	for (i = 0; i < num_threads; i++) {
		status = pthread_join(pthread_id[i], &value_ptr);
		if (status) {
			printf("    ERROR: pthread_join() status=%d\n", status);
			continue;
		} else {
			printf("    thread %d exited\n", (int)(uintptr_t)value_ptr);
		}
	}

	printf("TEST END:   Task Management\n");
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

// Returns the current time in seconds, as a double
static double
now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((double)tv.tv_sec + (double)tv.tv_usec * 1.e-6);
}


// This test migrates the hello_world process to every other available CPU,
// then back to the starting CPU. The first time through the ring prints
// out what is happening. After that, 100K ring traversals are performed
// and the average time per migration is printed.
static int 
task_migrate_test(void)
{
	int status, i, cur, cpu, home_cpu, home_index=-1, ncpus=0;
	int cpus[CPU_SETSIZE];
	cpu_set_t cpu_mask;
	int ntrips = 100000;
	double start, end;

	printf("\n");
	printf("TEST BEGIN: Task Migration Test\n");

	home_cpu = sched_getcpu();
	printf("  My home CPU is: %d\n",  home_cpu);

	status = pthread_getaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
	if (status) {
		printf("    ERROR: pthread_getaffinity_np() status=%d\n", status);
		return -1;
	}

	printf("  My CPU mask is: ");
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &cpu_mask)) {
			printf("%d ", i);
			cpus[ncpus] = i;
			++ncpus;
		}
	}
	printf("\n");

	printf("  Total # CPUs:   %d\n", ncpus);

	for (i = 0; i < ncpus; i++) {
		if (cpus[i] == home_cpu) {
			home_index = i;
			break;
		}
	}

	printf("  Begin Warm Up Trip Around Ring:\n");
	cur = (home_index + 1) % ncpus;
	printf("      Initial CPU is %d\n", home_cpu);
	for (i = 0; i < ncpus; i++) {
		printf("      Migrating to CPU %d: ", cpus[cur]);

		task_switch_cpus(cpus[cur]);

		cpu = sched_getcpu();
		if (cpu == cpus[cur])
			printf("Success\n");
		else
			printf("Failure (current CPU = %d)\n", cpu);

		cur = (cur + 1) % ncpus;
	}
	printf("      Trip Complete\n");

	printf("  Begin Benchmark (%d Trips Around Ring):\n", ntrips);
	start = now();
	cur = (home_index + 1) % ncpus;
	for (i = 0; i < (ntrips * ncpus); i++) {
		task_switch_cpus(cpus[cur]);
		cur = (cur + 1) % ncpus;
	}
	end = now();
	printf("      Average Time/Migrate: %.0f ns\n", (end - start) * 1.e9 / (ntrips * ncpus * 1.0));

	printf("TEST END:   Task Migration Test\n");
	return 0;
}


int
fd_test( void )
{
	char buf[ 1024 ] = "";
	int fd;
	ssize_t rc;

	printf("\n");
	printf("TEST BEGIN: File Test\n");

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

	printf("    Success.\n");
	printf("TEST END:   File Test\n");

	return 0;
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

	/* Make sure there is an embedded ISO image */
	if (iso_size != (iso_end - iso_start)) {
		//printf("    Failed, no ISO image available.\n");
		return -1;
	}

	printf("\n");
	printf("TEST BEGIN: Hypervisor API\n");

	printf("  Starting a guest OS...\n");

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

	printf("TEST END:   Hypervisor API\n");
	return 0;
}

/* Copyright (c) 2013, Sandia National Laboratories */

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

static int run_power_comm(void);

int
main(int argc, char *argv[], char *envp[])
{
	int i;

	printf("Power!\n");

	printf("Arguments:\n");
	for (i = 0; i < argc; i++)
		printf("  argv[%d] = %s\n", i, argv[i]);

	printf("Environment Variables:\n");
	for (i = 0; envp[i] != NULL; i++)
		printf("  envp[%d] = %s\n", i, envp[i]);

	return run_power_comm();
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
run_power_comm( void )
{
        printf("\nPower Communication (Proxy <=> Agent)\n");

        int pport = 20203, aport = 20202;
        int pfd = socket( PF_INET, SOCK_STREAM, 0 ),
            afd = socket( PF_INET, SOCK_STREAM, 0 );
        if( pfd < 0 || afd < 0 )
        {
                printf( "ERROR: socket() failed! rc=%d and %d\n", pfd, afd );
                return -1;
        }

        struct sockaddr_in paddr, aaddr, addr;
        socklen_t socklen = sizeof(aaddr);

        paddr.sin_family = AF_INET;
        paddr.sin_port = htons( pport );
        paddr.sin_addr.s_addr = INADDR_ANY;
        if( bind( pfd, (struct sockaddr*) &paddr, sizeof(paddr) ) < 0 )
                perror( "bind" );

        if( listen( pfd, 5 ) < 0 ) {
                perror( "listen" );
                return -1;
        }

        int max_fd = pfd;
        fd_set fds;
        FD_ZERO( &fds );
        FD_SET( pfd, &fds );

        printf( "Proxy listening on port %d\n", pport );

        while(1)
        {
                fd_set read_fds = fds;
                int rc = select( max_fd+1, &read_fds, 0, 0, 0 );
                if( rc < 0 )
                {
                        printf( "ERROR: select() failed! rc=%d\n", rc );
                        return -1;
                }

                if( FD_ISSET( pfd, &read_fds ) )
                {
                        printf( "Agent establishing connection\n" );
                        int new_fd = accept( pfd, (struct sockaddr *) &addr, &socklen );

			printf( "Agent IP address is %d.%d.%d.%d\n", 
				*((char *)(&addr.sin_addr.s_addr)+0),
				*((char *)(&addr.sin_addr.s_addr)+1),
				*((char *)(&addr.sin_addr.s_addr)+2),
				*((char *)(&addr.sin_addr.s_addr)+3) );

                        FD_SET( new_fd, &fds );
                        if( new_fd > max_fd )
                                max_fd = new_fd;

			sleep(2);

			printf( "Proxy establishing connection\n" );
			aaddr.sin_family = addr.sin_family;
			aaddr.sin_addr.s_addr = addr.sin_addr.s_addr;
			aaddr.sin_port = htons( aport );

			printf( "Connecting to agent address\n");
                        rc = connect( afd, (struct sockaddr *) &aaddr, sizeof(aaddr) );
                	if( rc < 0 )
                	{
                        	printf( "ERROR: connect() failed! rc=%d\n", rc );
                        	return -1;
                	}

			sleep(2);

                        char outbuf[ 32 ] = "", cmd[ 10 ] = "start";
			unsigned int val = 1;

			printf( "Setting agent to %s collection on port %u\n", cmd, val );
                        int len = snprintf( outbuf, sizeof(outbuf), "%s:%u\n", cmd, val );
			writen( afd, outbuf, len );
			close( afd );

			printf( "Successfully configured agent\n");
                }

                int fd;
                for( fd=0 ; fd<=max_fd ; fd++ )
                {
                        if( fd == pfd || fd == afd || !FD_ISSET( fd, &read_fds ) )
                                continue;

                        char buf[ 256 ];
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

                        printf( "%d: read %zd bytes: '%s'\n", fd, rc, buf);
                }
        }

        return 0;
}


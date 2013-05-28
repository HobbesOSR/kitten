/* Copyright (c) 2013, Sandia National Laboratories */

#include "piapi.h"

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

static int piapi_debug = 1;

struct piapi_context
{
	int pfd;
        fd_set fds;
	int max_fd;

	piapi_callback_t callback;
	pthread_t worker;
	int worker_run;
};

#define PIAPI_CNTX(X) ((struct piapi_context *)(X))

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


static int
piapi_proxy_listen( void *cntx )
{
	struct sockaddr_in addr;

	if( piapi_debug )
		printf( "Proxy establishing listener\n" );

        PIAPI_CNTX(cntx)->pfd = socket( PF_INET, SOCK_STREAM, 0 );
        if( PIAPI_CNTX(cntx)->pfd < 0 )
        {
                printf( "ERROR: socket() failed! rc=%d\n", PIAPI_CNTX(cntx)->pfd );
                return -1;
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons( PIAPI_PRXY_PORT );
        bind( PIAPI_CNTX(cntx)->pfd, (struct sockaddr*) &addr, sizeof(addr) );

        if( listen( PIAPI_CNTX(cntx)->pfd, 5 ) < 0 ) {
                perror( "listen" );
                return -1;
        }

        FD_SET( PIAPI_CNTX(cntx)->pfd, &PIAPI_CNTX(cntx)->fds );
        if( PIAPI_CNTX(cntx)->pfd < PIAPI_CNTX(cntx)->max_fd )
		PIAPI_CNTX(cntx)->max_fd = PIAPI_CNTX(cntx)->pfd;

	if( piapi_debug )
        	printf( "Proxy listening on port %d\n", PIAPI_PRXY_PORT );

	return 0;
}

static void
piapi_worker_thread( void *cntx )
{
	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);

	fd_set read_fds;
	int fd;

	char buf[ 256 ];
	ssize_t rc;

	PIAPI_CNTX(cntx)->worker_run = 1;
	while( PIAPI_CNTX(cntx)->worker_run )
	{
		read_fds = PIAPI_CNTX(cntx)->fds;
		rc = select( PIAPI_CNTX(cntx)->max_fd+1, &read_fds, 0, 0, 0 );
		if( rc < 0 )
		{
			printf( "ERROR: select() failed! rc=%d\n", rc );
			return;
		}

		if( FD_ISSET( PIAPI_CNTX(cntx)->pfd, &read_fds ) )
		{
			if( piapi_debug )
				printf( "Agent establishing connection\n" );

			fd = accept( PIAPI_CNTX(cntx)->pfd, (struct sockaddr *) &addr, &socklen );

			if( piapi_debug )
				printf( "Agent IP address is %d.%d.%d.%d\n",
					*((char *)(&addr.sin_addr.s_addr)+0),
					*((char *)(&addr.sin_addr.s_addr)+1),
					*((char *)(&addr.sin_addr.s_addr)+2),
					*((char *)(&addr.sin_addr.s_addr)+3) );

			FD_SET( fd, &PIAPI_CNTX(cntx)->fds );
			if( fd > PIAPI_CNTX(cntx)->max_fd )
				PIAPI_CNTX(cntx)->max_fd = fd;
		}

		for( fd=0 ; fd<=PIAPI_CNTX(cntx)->max_fd ; fd++ )
		{
			if( fd == PIAPI_CNTX(cntx)->pfd || !FD_ISSET( fd, &read_fds ) )
				continue;

			rc = read( fd, buf, sizeof(buf)-1 );
			if( rc <= 0 )
			{
				if( piapi_debug )
					printf( "%d: closed connection rc=%zd\n", fd, rc );
				FD_CLR( fd, &PIAPI_CNTX(cntx)->fds );
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

			if( piapi_debug )
				printf( "%d: read %zd bytes: '%s'\n", fd, rc, buf);

			if( PIAPI_CNTX(cntx)->callback )
				PIAPI_CNTX(cntx)->callback(buf, rc);
		}
	}
}

static int
piapi_control( void *cntx, char *cmd, char *val )
{
	struct sockaddr_in addr;
	int afd = socket( PF_INET, SOCK_STREAM, 0 );
	char outbuf[ 32 ] = "";
	unsigned int len;
	int rc;

	if( piapi_debug )
		printf( "Proxy establishing control connection\n" );

	if( afd < 0 )
	{
		printf( "ERROR: socket() failed! rc=%d\n", afd );
		return -1;
	}

        addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl( PIAPI_AGNT_SADDR );
	addr.sin_port = htons( PIAPI_AGNT_PORT );

	if( piapi_debug )
		printf( "Agent IP address is %d.%d.%d.%d\n",
			*((char *)(&addr.sin_addr.s_addr)+0),
			*((char *)(&addr.sin_addr.s_addr)+1),
			*((char *)(&addr.sin_addr.s_addr)+2),
			*((char *)(&addr.sin_addr.s_addr)+3) );

	rc = connect( afd, (struct sockaddr *) &addr, sizeof(addr) );
	if( rc < 0 )
	{
		printf( "ERROR: connect() failed! rc=%d\n", rc );
		perror( "CONNECT:" );
		return -1;
	}

	if( piapi_debug )
		printf( "Sending control command to agent %s:%s\n", cmd, val );

	len = snprintf( outbuf, sizeof(outbuf), "%s:%s\n", cmd, val );
	writen( afd, outbuf, len );

	close( afd );

	if( piapi_debug )
		printf( "Successfully configured agent\n");

	return 0;
}

static int
piapi_attach( void *cntx )
{
	char cmd[ 10 ] = "attach";

	if( piapi_debug )
		printf( "Setting agent to %s to proxy at %s\n", cmd, PIAPI_PRXY_IPADDR);

	if( piapi_control( cntx, cmd, PIAPI_PRXY_IPADDR ) < 0 )
	{
		printf( "ERROR: Control message failed\n");
		return -1;
	}

	if( piapi_debug )
		printf( "Successfully attached agent\n");

	return 0;
}

static int
piapi_detach( void *cntx )
{
	char cmd[ 10 ] = "detach";

	if( piapi_debug )
		printf( "Setting agent to %s from proxy at %s\n", cmd, PIAPI_PRXY_IPADDR);

	if( piapi_control( cntx, cmd, PIAPI_PRXY_IPADDR ) < 0 )
	{
		printf( "ERROR: Control message failed\n");
		return -1;
	}

	if( piapi_debug )
		printf( "Successfully detached agent\n");

	return 0;
}

int
piapi_init( void **cntx, piapi_callback_t *callback )
{
	if( piapi_debug )
        	printf("\nPower Communication (Proxy <=> Agent)\n");

	*cntx = malloc( sizeof(struct piapi_context) );
	bzero( *cntx, sizeof(struct piapi_context) );

	PIAPI_CNTX(*cntx)->callback = *callback;
        FD_ZERO( &PIAPI_CNTX(*cntx)->fds );

	if( piapi_proxy_listen( *cntx ) )
	{
		printf( "ERROR: unable to start proxy\n" );
		return -1;
	}

	if( piapi_debug )
       		printf( "Proxy listener established\n" );

	if( piapi_attach( cntx ) < 0 )
	{
		printf( "ERROR: Unable to attach agent\n");
		return -1;
	}

	if( piapi_debug )
       		printf( "Agent connection established\n" );

	pthread_create(&(PIAPI_CNTX(*cntx)->worker), 0x0, (void *)&piapi_worker_thread, *cntx);
	return 0;
}

int
piapi_destroy( void *cntx )
{
	PIAPI_CNTX(cntx)->worker_run = 0;

	if( piapi_detach( cntx ) < 0 )
	{
		printf( "ERROR: Unable to detach agent\n");
		return -1;
	}

	close( PIAPI_CNTX(cntx)->pfd );

	return 0;
}

int
piapi_start( void *cntx, piapi_port_t port )
{
	char cmd[ 10 ] = "start";
	char val[10] = "";

	if( piapi_debug )
		printf( "Setting agent to %s collection on sensor port %u\n", cmd, port);

	snprintf( val, 10, "%u", port );
	if( piapi_control( cntx, cmd, val ) < 0 )
	{
		printf( "ERROR: Control message failed\n");
		return -1;
	}

	if( piapi_debug )
		printf( "Successfully started agent\n");

	return 0;
}

int
piapi_stop( void *cntx, piapi_port_t port )
{
	char cmd[ 10 ] = "stop";
	char val[10] = "";

	if( piapi_debug )
		printf( "Setting agent to %s collection on sensor port %u\n", cmd, port);

	snprintf( val, 10, "%u", port );
	if( piapi_control( cntx, cmd, val ) < 0 )
	{
		printf( "ERROR: Control message failed\n");
		return -1;
	}

	if( piapi_debug )
		printf( "Successfully stopped agent\n");

	return 0;
}


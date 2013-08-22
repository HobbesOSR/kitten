/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2009, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it under the terms of the GNU General Public License
 * Version 2 (GPLv2).  The accompanying COPYING file contains the
 * full text of the license.
 */

#include <interfaces/vmm_socket.h>
#include <lwip/init.h>
#include <lwip/tcp.h>
#include <lwip/api.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <lwk/driver.h>

typedef int palacios_socket;

//ignore the arguments given here currently
static void *
palacios_tcp_socket(
	const int bufsize,
	const int nodelay,
	const int nonblocking,
	void * private_data
)
{
	int sock = lwip_socket(PF_INET, SOCK_STREAM, 0);
	
	if (nodelay) {
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		           &nodelay, sizeof(nodelay));
	}

	palacios_socket *sock_ptr = (palacios_socket *) kmem_alloc(sizeof(palacios_socket));  
	*sock_ptr = sock;

	return sock_ptr;
}

//ignore the arguments given here currently
static void *
palacios_udp_socket(
	const int bufsize,
	const int nonblocking,
	void * private_data
)
{
  	int sock = lwip_socket(PF_INET, SOCK_DGRAM, 0);
    
	palacios_socket *sock_ptr = (palacios_socket *) kmem_alloc(sizeof(palacios_socket));  
	*sock_ptr = sock;

	return sock_ptr;
}

static void 
palacios_close(void * sock_ptr)
{
	palacios_socket *sock = (palacios_socket *) sock_ptr;
  	lwip_close(*sock);
	kmem_free(sock);
}

static int 
palacios_bind_socket(
	const void * sock_ptr,
	const int port
)
{
  	struct sockaddr_in addr;

  	addr.sin_family = AF_INET;
  	addr.sin_port = htons( port );
  	addr.sin_addr.s_addr = INADDR_ANY;
    
	palacios_socket *sock = (palacios_socket *) sock_ptr; 

  	return lwip_bind(*sock, (struct sockaddr *) &addr, sizeof(addr));
}

static int 
palacios_listen(
	const void * sock_ptr,
	int backlog
)
{
	palacios_socket *sock = (palacios_socket *) sock_ptr;
  	return lwip_listen(*sock, backlog);
}

static void *
palacios_accept(
	const void * sock_ptr,
	unsigned int * remote_ip,
	unsigned int * port
)
{
  	struct sockaddr_in client;
  	socklen_t len;
  	int client_sock;
	palacios_socket *sock = (palacios_socket *) sock_ptr;
  	client_sock = lwip_accept(*sock, (struct sockaddr *) &client, &len);

  	if (client.sin_family == AF_INET) {
		*port      = ntohs (client.sin_port);
		*remote_ip = ntohl(client.sin_addr.s_addr);
	}

	palacios_socket *client_sock_ptr = (palacios_socket *) kmem_alloc(sizeof(palacios_socket));
	*client_sock_ptr = client_sock;

	return client_sock_ptr;
}

static int 
palacios_select(
	struct v3_sock_set * rset,
	struct v3_sock_set * wset,
	struct v3_sock_set * eset,
	struct v3_timeval tv)
{
  	//! \todo fixup computation of n
  	return lwip_select(16, (fd_set *)rset, (fd_set *)wset, (fd_set *)eset, (struct timeval *)&tv);
}

static int 
palacios_connect_to_ip(
	const void * sock_ptr,
	const int hostip,
	const int port
)
{
  	struct sockaddr_in client;

  	client.sin_family = AF_INET;
  	client.sin_port = htons( port );
  	client.sin_addr.s_addr = htonl(hostip);

	palacios_socket *sock = (palacios_socket *) sock_ptr;

  	return lwip_connect(*sock, (struct sockaddr *) &client, sizeof(client));
}

static int 
palacios_send(
	const void * sock_ptr,
	const char * buf,
	const int len
)
{
	palacios_socket *sock = (palacios_socket *) sock_ptr;
  	return lwip_write(*sock, buf, len);
}

static int 
palacios_recv(
	const void * sock_ptr,
	char * buf,
	const int len
)
{
	palacios_socket *sock = (palacios_socket *) sock_ptr;
  	return lwip_read(*sock, buf, len);
}

static int 
palacios_sendto_ip(
	const void * sock_ptr,
	const int ip_addr,
	const int port,
	const char * buf,
	const int len
)
{
	struct sockaddr_in dst;

	dst.sin_family = AF_INET;
	dst.sin_port = htons(port);
	dst.sin_addr.s_addr = htonl(ip_addr);
	palacios_socket *sock = (palacios_socket *) sock_ptr;
	return lwip_sendto(*sock, buf, len, 0, (struct sockaddr *) &dst, sizeof(dst));
}

static int 
palacios_recvfrom_ip(
	const void * sock_ptr,
	const int ip_addr,
	const int port,
	char * buf,
	const int len
)
{
  	struct sockaddr_in src;
  	int alen;

  	src.sin_family = AF_INET;
  	src.sin_port = htons(port);
  	src.sin_addr.s_addr = htonl(ip_addr);
  	alen = sizeof(src);
	palacios_socket *sock = (palacios_socket *) sock_ptr;
  	return lwip_recvfrom(*sock, buf, len, 0 /*unsigned int flags*/, (struct sockaddr *) &src, &alen);
}

struct v3_socket_hooks palacios_sock_hooks = {
  	.tcp_socket = palacios_tcp_socket,
  	.udp_socket = palacios_udp_socket,
  	.close = palacios_close,
  	.bind = palacios_bind_socket,
  	.listen = palacios_listen,
  	.accept = palacios_accept,
  	.select = palacios_select,
  	.connect_to_ip = palacios_connect_to_ip,
  	.connect_to_host = NULL,
  	.send = palacios_send,
  	.recv = palacios_recv,
  	.sendto_host = NULL,
  	.sendto_ip = palacios_sendto_ip,
  	.recvfrom_host = NULL,
  	.recvfrom_ip = palacios_recvfrom_ip,
};

int 
palacios_socket_init(void) 
{
  	V3_Init_Sockets(&palacios_sock_hooks);
	return 0;
}

DRIVER_INIT("module", palacios_socket_init);

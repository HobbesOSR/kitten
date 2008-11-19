/** \file
 * Network device and TCP/IP stack initialization.
 */
#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/netdev.h>
#include <lwk/kmem.h>
#include <lwip/init.h>
#include <lwip/tcp.h>
#include <lwip/api.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <arch/uaccess.h>
#include <arch/unistd.h>
#include <arch/vsyscall.h>

/**
 * Holds a comma separated list of network devices to configure.
 */
static char netdev_str[128];
param_string(net, netdev_str, sizeof(netdev_str));


struct tcp_pcb * echo;
struct echo_context {
	char buf[ 128 ];
	unsigned head;
	unsigned tail;
};


static err_t
echo_recv(
	void *			arg,
	struct tcp_pcb *	pcb,
	struct pbuf *		p,
	err_t			err
)
{
	struct echo_context *	context = arg;
	if( !p )
	{
		if( err == ERR_OK )
		{
			// Connection closed
			printk( "%s: They closed on us\n", __func__ );
			kmem_free( context );
		}

		return ERR_OK;
	}
		
	printk( "%s: %d bytes: '%s'\n",
		__func__,
		p->tot_len,
		(const char*) p->payload
	);

	// Acknowledge that we have received the entire payload
	// and that we're done with the packet
	tcp_recved( pcb, p->tot_len );
	pbuf_free( p );

	return ERR_OK;
}


static err_t
echo_accept(
	void *			priv,
	struct tcp_pcb *	pcb,
	err_t			err
)
{
	printk( "%s: Accepting a new connection\n", __func__ );
	struct echo_context * context = kmem_alloc( sizeof(*context) );
	context->head = context->tail = 0;

	tcp_setprio( pcb, TCP_PRIO_MIN );
	tcp_arg( pcb, context );
	tcp_recv( pcb, echo_recv );
	//tcp_err( pcb, echo_err );
	//tcp_poll( pcb, echo_poll, 4 );

	return ERR_OK;
}


static unsigned long
sys_bind(
	int			sock,
	uaddr_t			addr,
	size_t			len
)
{
	if( len > 128 )
		return -ENAMETOOLONG;

	uint8_t buf[ len ];
	if( copy_from_user( buf, (void*) addr, len ) )
	{
		printk( "%s: bad user address %p\n", __func__, (void*) addr );
		return -EFAULT;
	}

	return lwip_bind( sock, (struct sockaddr *) buf, len );
}



int
sys_select(
	int			n,
	fd_set *		user_readfds,
	fd_set *		user_writefds,
	fd_set *		user_exceptfds,
	struct timeval *	user_timeout
)
{
	fd_set readfds, writefds, exceptfds;
	struct timeval timeout;

	if( user_readfds
	&& copy_from_user( &readfds, user_readfds, sizeof(readfds) ) )
		return -EBADF;

	if( user_writefds
	&& copy_from_user( &writefds, user_writefds, sizeof(writefds) ) )
		return -EBADF;

	if( user_exceptfds
	&& copy_from_user( &exceptfds, user_exceptfds, sizeof(exceptfds) ) )
		return -EBADF;

	if( user_timeout
	&& copy_from_user( &timeout, user_timeout, sizeof(timeout) ) )
		return -EBADF;

	int rc = lwip_select(
		n,
		user_readfds ? &readfds : 0,
		user_writefds ? &writefds : 0,
		user_exceptfds ? &exceptfds : 0,
		user_timeout ? &timeout : 0
	);


	if( user_readfds
	&& copy_to_user( user_readfds, &readfds, sizeof(readfds) ) )
		return -EBADF;
	if( user_writefds
	&& copy_to_user( user_writefds, &writefds, sizeof(writefds) ) )
		return -EBADF;
	if( user_exceptfds
	&& copy_to_user( user_exceptfds, &exceptfds, sizeof(exceptfds) ) )
		return -EBADF;

	return rc;
}

	

/**
 * Initializes the network subsystem; called once at boot.
 */
void
netdev_init(void)
{
	printk( KERN_INFO "%s: Bringing up network devices: '%s'\n",
		__func__,
		netdev_str
	);

	driver_init_list( "net", netdev_str );

/*
	echo = tcp_new();
	if( !echo )
		panic( "%s: Unable to create socket!\n", __func__ );
	if( tcp_bind( echo, IP_ADDR_ANY, 7 ) != ERR_OK )
		panic( "%s: Bind failed\n" );

	echo = tcp_listen( echo );
	tcp_accept( echo, echo_accept );

	printk( "%s: pcb %p port %d\n",
		__func__,
		echo,
		echo->local_port
	);
*/

	tcpip_init( 0, 0 );

	// Install the socket system calls
	syscall_register( __NR_read, (syscall_ptr_t) lwip_read );
	syscall_register( __NR_socket, (syscall_ptr_t) lwip_socket );
	syscall_register( __NR_bind, (syscall_ptr_t) sys_bind );
	syscall_register( __NR_accept, (syscall_ptr_t) lwip_accept );
	syscall_register( __NR_listen, (syscall_ptr_t) lwip_listen );
	syscall_register( __NR_select, (syscall_ptr_t) sys_select );
}


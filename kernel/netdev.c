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
	printk( "%s: sock %d addr %p len %ld\n",
		__func__,
		sock,
		(void*) addr,
		len
	);

	if( len > 128 )
		return -ENAMETOOLONG;

	uint8_t buf[ len ];
	if( copy_from_user( buf, (void*) addr, len ) )
	{
		printk( "%s: bad user address %p\n", __func__, (void*) addr );
		return -EFAULT;
	}

	printk( "'%s'\n", hexdump( buf, len ) );
	struct sockaddr_in * in = (void*) buf;
	printk( "sizeof(in)=%ld family=%d port=%d\n",
		sizeof(*in),
		in->sin_family,
		ntohs( in->sin_port )
	);

	return lwip_bind( sock, (struct sockaddr *) buf, len );
}


/**
 * Initializes the network subsystem; called once at boot.
 */
void
netdev_init(void)
{
	lwip_init();

	printk( KERN_INFO "%s: Bringing up network devices: '%s'\n",
		__func__,
		netdev_str
	);

	driver_init_list( "net", netdev_str );

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

	// Install the socket system calls
	syscall_register( __NR_socket, (syscall_ptr_t) lwip_socket );
	syscall_register( __NR_bind, (syscall_ptr_t) sys_bind );
	syscall_register( __NR_accept, (syscall_ptr_t) lwip_accept );
	syscall_register( __NR_listen, (syscall_ptr_t) lwip_listen );
}


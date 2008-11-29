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
#include <lwk/kfs.h>


/**
 * Holds a comma separated list of network devices to configure.
 */
static char netdev_str[128];
param_string(net, netdev_str, sizeof(netdev_str));

#ifdef CONFIG_LWIP_SOCKET

/** Return a LWIP connection number from a user fd */
static inline int
lwip_connection(
	const struct kfs_file *	file
)
{
	return (int)(uintptr_t) file->priv;
}


static inline int
lwip_from_fd(
	int			fd
)
{
	struct kfs_file *	file = get_current_file( fd );
	if( !file )
		return -1;
	//if( file->fops != &kfs_socket_fops )
		//return -1;
	return lwip_connection( file );
}


static ssize_t
socket_write(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	char			kbuf[ 512 ];
	size_t			klen = len;
	if( klen > sizeof(kbuf)-1 )
		klen = sizeof(kbuf)-1;

	if( copy_from_user( kbuf, (void*) buf, klen ) )
		return -EFAULT;

	return lwip_write( lwip_connection(file), kbuf, klen );
}


static ssize_t
socket_read(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			count
)
{
	return lwip_read( lwip_connection(file), (void*) buf, count );
}

static int
sys_listen(
	int			fd,
	int			backlog
)
{
	return lwip_listen( lwip_from_fd(fd), backlog );
}



static int
socket_close(
	struct kfs_file *	file
)
{
	printk( "%s: Closing file %p\n", __func__, file );
	lwip_close( lwip_connection( file ) );
	kfs_destroy( file );
	return 0;
}


struct kfs_fops kfs_socket_fops = {
	.write		= socket_write,
	.read		= socket_read,
	.close		= socket_close,
};

static int
socket_allocate( void )
{
	int fd;
	for( fd=0 ; fd < MAX_FILES ; fd++ )
	{
		if( !current->files[ fd ] )
			break;
	}

	if( fd == MAX_FILES )
		return -EMFILE;

	current->files[ fd ] = kfs_mkdirent(
		NULL,
		"sock",
		&kfs_socket_fops,
		0777,
		0,
		0
	);

	printk( "%s: New socket fd %d file %p\n", __func__, fd, current->files[ fd ] );

	return fd;
}



static int
sys_socket(
	int			domain,
	int			type,
	int			protocol
)
{
	// Allocate a fd for this one
	int fd = socket_allocate();
	struct kfs_file * file = get_current_file( fd );
	if( !file )
		return -EMFILE;

	int conn = lwip_socket( domain, type, protocol );
	if( conn < 0 )
	{
		current->files[ fd ] = 0;
		kfs_destroy( file );
		return conn;
	}

	file->priv = (void*)(uintptr_t) conn;
	return fd;
}




static unsigned long
sys_bind(
	int			sock,
	uaddr_t			addr,
	size_t			len
)
{
	const int conn = lwip_from_fd( sock );
	if( sock < 0 )
		return -EBADF;

	if( len > 128 )
		return -ENAMETOOLONG;

	uint8_t buf[ len ];
	if( copy_from_user( buf, (void*) addr, len ) )
	{
		printk( "%s: bad user address %p\n", __func__, (void*) addr );
		return -EFAULT;
	}

	return lwip_bind( conn, (struct sockaddr *) buf, len );
}


static int
sys_accept(
	int			fd,
	struct sockaddr *	addr,
	socklen_t *		addrlen
)
{
	int new_fd = socket_allocate();
	struct kfs_file * new_file = get_current_file( new_fd );
	if( !new_file )
		return -EMFILE;

	int conn = lwip_accept( lwip_from_fd(fd), addr, addrlen );
	if( conn < 0 )
	{
		current->files[new_fd] = 0;
		kfs_destroy( new_file );
		return conn;
	}

	new_file->priv = (void*)(uintptr_t) conn;
	return new_fd;
}


static int
translate_in_fdset(
	int			n,
	fd_set * const		out_fds,
	const fd_set * const	user_in_fds
)
{
	fd_set in_fds;
	FD_ZERO( out_fds );

	if( !user_in_fds )
		return 0;

	if( copy_from_user( &in_fds, user_in_fds, sizeof(in_fds) ) )
		return -EBADF;

	int fd;
	for( fd=0 ; fd<n ; fd++ )
	{
		if( !FD_ISSET( fd, &in_fds ) )
			continue;
		int conn = lwip_from_fd( fd );
		if( conn < 0 )
			return -EBADF;

		FD_SET( conn, out_fds );
	}

	return 0;
}


static int
translate_out_fdset(
	int			n,
	fd_set * const		user_out_fds,
	const fd_set * const	in_fds
)
{
	fd_set out_fds, old_out_fds;
	FD_ZERO( &out_fds );
	if( !user_out_fds )
		return 0;

	if( copy_from_user( &old_out_fds, user_out_fds, sizeof(old_out_fds) ) )
		return -EBADF;

	int fd;
	for( fd=0 ; fd<n ; fd++ )
	{
		if( !FD_ISSET( fd, &old_out_fds ) )
			continue;

		// fd was set in the original input to select, translate
		// it to a connection and check that connection in the
		// result from select.
		int conn = lwip_from_fd( fd );
		if( conn < 0 )
			return -EBADF;
		if( FD_ISSET( conn, in_fds ) )
			FD_SET( fd, &out_fds );
	}

	if( copy_to_user( user_out_fds, &out_fds, sizeof(out_fds) ) )
		return -EBADF;

	return 0;
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
	fd_set readfds, writefds;
	struct timeval timeout;

	if( translate_in_fdset( n, &readfds, user_readfds ) < 0 )
		return -EBADF;

	if( translate_in_fdset( n, &writefds, user_writefds ) < 0 )
		return -EBADF;

	// Except fds are not supported
			
	if( user_timeout
	&& copy_from_user( &timeout, user_timeout, sizeof(timeout) ) )
		return -EBADF;

	//! \todo fixup computation of n
	int rc = lwip_select(
		16,
		&readfds,
		&writefds,
		0,
		user_timeout ? &timeout : 0
	);

	if( translate_out_fdset( n, user_readfds, &readfds ) < 0 )
		return -EBADF;

	if( translate_out_fdset( n, user_writefds, &writefds ) < 0 )
		return -EBADF;

	return rc;
}


#endif // CONFIG_SOCKET

	

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

#ifdef CONFIG_LWIP_SOCKET
	// Full scokets are enabled.  Bring up the entire system
	tcpip_init( 0, 0 );

	// Install the socket system calls
	syscall_register( __NR_socket, (syscall_ptr_t) sys_socket );
	syscall_register( __NR_bind, (syscall_ptr_t) sys_bind );
	syscall_register( __NR_accept, (syscall_ptr_t) sys_accept );
	syscall_register( __NR_listen, (syscall_ptr_t) sys_listen );
	syscall_register( __NR_select, (syscall_ptr_t) sys_select );
#elif defined( CONFIG_NETWORK )
	// No sockets enabled, just bring up the LWIP library
	lwip_init();
#endif
}


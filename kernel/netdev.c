#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/netdev.h>
#include <lwk/kmem.h>
#include <arch/uaccess.h>
#include <arch/unistd.h>
#include <arch/vsyscall.h>
#include <lwk/kfs.h>
#include <lwk/fdTable.h>

/**
 * Holds a comma separated list of network devices to configure.
 */
static char netdev_str[128];
param_string(net, netdev_str, sizeof(netdev_str));

#ifdef CONFIG_LWIP_SOCKET

#include <lwip/init.h>
#include <lwip/tcp.h>
#include <lwip/api.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <lwip/stats.h>

/** Return a LWIP connection number from a user fd */
static inline int
lwip_connection(
	const struct file *	file
)
{
	return (int)(uintptr_t) file->private_data;
}


static inline int
lwip_from_fd(
	int			fd
)
{
	struct file *		file = get_current_file( fd );
	if( !file )
		return -1;
	//if( file->fops != &kfs_socket_fops )
		//return -1;
	return lwip_connection( file );
}


static ssize_t
socket_write(
	struct file *		file,
	const char *		buf,
	size_t			len,
	loff_t *		off
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
	struct file *		file,
	char *			buf,
	size_t			count,
	loff_t *		off
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
	struct file *		file
)
{
	printk( "%s: Closing file %p\n", __func__, file );
	#ifdef LWIP_STATS_DISPLAY
		stats_display();
	#endif
	lwip_close(lwip_connection(file));
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
	int fd = fdTableGetUnused(current->fdTable);

	struct file * file = kmem_alloc( sizeof(*file));
	if( !file )
		return -EMFILE;

	memset(file,0,sizeof(*file));
	file->f_op = &kfs_socket_fops;
	atomic_set(&file->f_count,1);

        fdTableInstallFd( current->fdTable, fd, file );

	printk( "%s: New socket fd %d file %p\n", __func__, fd, file );

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
	struct file * file = get_current_file( fd );
	if( !file )
		return -EMFILE;

        fdTableInstallFd( current->fdTable, fd, file );

	int conn = lwip_socket( domain, type, protocol );
	if( conn < 0 )
	{
        	fdTableInstallFd( current->fdTable, fd, 0 );
		kmem_free( file );
		return conn;
	}

	file->private_data = (void*)(uintptr_t) conn;
	return fd;
}

static int
translate_sockaddr(
	uint8_t *		buf,
	size_t			len
)
{
	typedef unsigned short sa_family_t;
	struct linux_sockaddr {
		sa_family_t sa_family;
		char sa_data[14];
	};

	struct sockaddr addr;

	if( len < sizeof(struct sockaddr_in) )
		return -1;

	addr.sa_len = 0;
	addr.sa_family = ((struct linux_sockaddr *)buf)->sa_family;
	memcpy(addr.sa_data, ((struct linux_sockaddr *)buf)->sa_data, 14);

	memcpy(buf, &addr, sizeof(struct sockaddr));

	return 0;
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

	if( translate_sockaddr( buf, len ) < 0 )
	{
		printk( "%s: bad user address translation %p\n", __func__, (void*) addr );
		return -EFAULT;
	}

	return lwip_bind( conn, (struct sockaddr *) buf, len );
}


static unsigned long
sys_connect(
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

	if( translate_sockaddr( buf, len ) < 0 )
	{
		printk( "%s: bad user address %p translation\n", __func__, (void*) addr );
		return -EFAULT;
	}

	return lwip_connect( conn, (struct sockaddr *) buf, len );
}

static int
sys_accept(
	int			fd,
	struct sockaddr *	addr,
	socklen_t *		addrlen
)
{
	int new_fd = socket_allocate();
	struct file * new_file = get_current_file( new_fd );
	if( !new_file )
		return -EMFILE;

 
	int conn = lwip_accept( lwip_from_fd(fd), addr, addrlen );
	if( conn < 0 )
	{
        	fdTableInstallFd( current->fdTable, new_fd, 0 );
		kmem_free( new_file );
		return conn;
	}

	new_file->private_data = (void*)(uintptr_t) conn;
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

static int
sys_setsockopt(
	int				sockfd, 
	int				level,
	int				optname,
	const void * 	optval,
	socklen_t		optlen
)
{
	char			kbuf[ optlen + 1 ];

	const int conn = lwip_from_fd( sockfd );
	if( sockfd < 0 )
		return -EBADF;

	if( copy_from_user( kbuf, optval, optlen ) )
		return -EFAULT;

	/* printk( KERN_INFO "%s: Inside sys_setsockopt level=%d, optname=%d, optval=%s, optlen=%d\n", __func__, level, optname, (char *)optval, optlen); */
	return setsockopt(conn, level, optname, kbuf, optlen);
}

static int
sys_getsockopt(
	int				sockfd, 
	int				level,
	int				optname,
	void * 	optval,
	socklen_t *		optlen
)
{
	int			kint;
	int ret;

	const int conn = lwip_from_fd( sockfd );
	if( sockfd < 0 )
		return -EBADF;

	/* printk( KERN_INFO "%s: Inside sys_getsockopt level=%d, optname=%d, optval=%s, optlen=%d\n", __func__, level, optname, (char *)optval, optlen); */
	ret = getsockopt(conn, level, optname, &kint, optlen);

	if( copy_to_user( optval, &kint, *optlen ) ) {
		return -EFAULT;
	}

	return(ret);
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
	// Full sockets are enabled.  Bring up the entire system
	tcpip_init( 0, 0 );

	// Install the socket system calls
	syscall_register( __NR_socket, (syscall_ptr_t) sys_socket );
	syscall_register( __NR_bind, (syscall_ptr_t) sys_bind );
	syscall_register( __NR_connect, (syscall_ptr_t) sys_connect );
	syscall_register( __NR_accept, (syscall_ptr_t) sys_accept );
	syscall_register( __NR_listen, (syscall_ptr_t) sys_listen );
	syscall_register( __NR_select, (syscall_ptr_t) sys_select );
	syscall_register( __NR_setsockopt, (syscall_ptr_t) sys_setsockopt );
	syscall_register( __NR_getsockopt, (syscall_ptr_t) sys_getsockopt );
#elif defined( CONFIG_NETWORK )
	// No sockets enabled, just bring up the LWIP library
	lwip_init();
#endif
}


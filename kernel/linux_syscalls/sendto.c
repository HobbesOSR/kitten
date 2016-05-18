/* TODO: This is stubbed out to make libevent happy. */

#include <lwk/kernel.h>
#include <arch/unistd.h>

typedef uint32_t socklen_t;
struct sockaddr {};

extern ssize_t sys_write(int fd, uaddr_t buf, size_t len);

ssize_t
sys_sendto(int sockfd, uaddr_t buf, size_t len, int flags,
           const struct sockaddr *dest_addr, socklen_t addrlen)
{
	printk("%s: sendtoing sockfd %d len %lu\n", __func__, sockfd, (unsigned long)len);
	return sys_write(sockfd, buf, len);
}

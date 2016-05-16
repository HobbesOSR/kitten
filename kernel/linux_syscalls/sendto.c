#include <lwk/kernel.h>

typedef int socklen_t;

ssize_t sys_sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen)
{
  printk("%s: sendtoing sockfd %d buf %*s\n", __func__, sockfd, len, buf);
  return sys_write(sockfd, buf, len);
}

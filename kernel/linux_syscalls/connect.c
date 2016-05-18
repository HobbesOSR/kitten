/* TODO: This is stubbed out to make libevent happy. */

#include <lwk/kernel.h>

typedef uint32_t socklen_t;
struct sockaddr {};

int
sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	return 0;
}

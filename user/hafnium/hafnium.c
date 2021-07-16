/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <lwk/liblwk.h>
#include <arch/types.h>
#include <lwk/pmem.h>
#include <lwk/smp.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <poll.h>

#include <stdint.h>

#include "hafnium.h"






int
main(int argc, char ** argv, char * envp[]) 
{

	int hafnium_fd = 0;
	long ret;


	printf("Hafnium Control Daemon\n");


	hafnium_fd = open(HAFNIUM_CMD_PATH, O_RDWR);

	if (hafnium_fd < 0) {
		printf("Error opening hafnium cmd file (%s)\n", HAFNIUM_CMD_PATH);
		return -1;
	}


	ret = ioctl(hafnium_fd, HAFNIUM_IOCTL_HYP_INIT, NULL);

	while (1) {
#if 0
		int ret = 0;

		ret = read(hafnium_fd, &cmd, sizeof(struct pisces_cmd));

		if (ret != sizeof(struct pisces_cmd)) {
			printf("Error reading pisces CMD (ret=%d)\n", ret);
			break;
		}

		//printf("Command=%llu, data_len=%d\n", cmd.cmd, cmd.data_len);
#endif
		sleep(5);
	}

	close(hafnium_fd);

	return 0;
}

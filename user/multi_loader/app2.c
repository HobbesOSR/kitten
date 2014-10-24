#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>

int main(int argc, char **argv)
{
	FILE *fd;
	char str[1024];

	printf("Hello world from app 2\n");

	if ((fd = fopen("/tmp/file2", "r")) == NULL) {
		printf("Failed to open file\n");
		goto end;
	}

	if (fgets(str, sizeof(str), fd) == NULL) {
		printf("Failed to read from file\n");
		goto end;
	}

	printf("Read from /tmp/file2: %s\n", str);

end:
	// Sleep "forever"
	while (1)
		sleep(100000);
}

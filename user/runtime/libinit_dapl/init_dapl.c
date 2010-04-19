#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static char* dat_conf = 
"ofa-v2-mlx4_0-1 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 \"mlx4_0 1\" \"\"\n"
"ofa-v2-mlx4_0-2 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 \"mlx4_0 2\" \"\"\n"
"ofa-v2-ib0 u2.0 nonthreadsafe default libdaplofa.so.2 dapl.2.0 \"ib0 0\" \"\"\n"
"ofa-v2-ib1 u2.0 nonthreadsafe default libdaplofa.so.2 dapl.2.0 \"ib1 0\" \"\"\n";

static __attribute__ ((constructor (65534))) void init(void)
{
	struct stat buf;
	int ret;

	printf("creating /etc/dat.conf\n");
	
	if ( ( ret = stat("/",&buf) ) ) {
		printf("stat / failed %s\n",strerror(ret));
		while(1);
	}

	if ( ( ret = stat( "/etc", &buf) ) ) {
		printf("stat /etc failed %s\n",strerror(ret));
		if ( ( ret = mkdir( "/etc", 0x777 ) ) ) {
			printf("mkdir /etc failed %s\n",strerror(ret));
			while(1);
		}
	}

//	printf("dat.conf-> \n%s", dat_conf);

	int fd;
	if ( ( fd = open("/etc/dat.conf", O_RDWR ) ) == -1 ) {
		printf("open /etc/dat.conf failed %s\n",strerror(ret));
		while(1);
	}

	if ( write( fd, dat_conf, strlen(dat_conf) ) != strlen(dat_conf) ) {
		printf("write failed %s\n",strerror(ret));
		while(1);
	}

	close(fd);
}


#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char* mca_btl_openib_hca_params_ini =
"[default]\n"
"vendor_id = 0\n"
"vendor_part_id = 0\n"
"use_eager_rdma = 0\n"
"mtu = 1024\n"
"[Mellanox Hermon]\n"
"vendor_id = 0x2c9,0x5ad,0x66a,0x8f1,0x1708,0x03ba\n"
"vendor_part_id = 25408,25418,25428,26418,26428\n"
"use_eager_rdma = 1\n"
"mtu = 2048\n"
;

static char* openmpi_mca_params_conf =
"orte_debug = 0\n"
"btl_base_debug = 0\n"
;

static void foo( char* filename, char* data )
{
        int ret;
        printf("creating %s\n",filename);

	printf("%s\n %s", filename, data);

        int fd;
        if ( ( fd = open(filename, O_RDWR ) ) == -1 ) {
                printf("open %s failed %s\n",filename,strerror(ret));
                while(1);
        }

        if ( write( fd, data, strlen(data) ) != strlen(data) ) {
                printf("write failed %s\n",strerror(ret));
                while(1);
        }

        close(fd);
}

static __attribute__ ((constructor (65535))) void init(void)
{
	foo( "/etc/opal/mca-btl-openib-hca-params.ini",
		mca_btl_openib_hca_params_ini );

	foo( "/etc/opal/openmpi-mca-params.conf",
		openmpi_mca_params_conf );
}

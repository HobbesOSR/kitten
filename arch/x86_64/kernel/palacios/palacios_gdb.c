#include <interfaces/vmm_gdb.h>

#include <lwk/kfs.h>
#include <lwk/gdb.h>
#include <lwk/gdbio.h>
#include <lwk/driver.h>

#include <arch-generic/fcntl.h>

static void * palacios_gdb_connect(char *gdbcons_backend){
	return gdbio_connect(gdbcons_backend);
}

static int palacios_gdb_disconnect(void * file_ptr){
	struct file *fd = (struct file *) file_ptr;
	static char *detach_pkt = "$D#44";
	gdbclient_write(fd, detach_pkt, strlen(detach_pkt));

	char buf[3];
	memset(buf, 0, sizeof(buf));
	while(1){
		gdbclient_read(fd, buf, 2);
		buf[2] = '\0';
		if(strcmp(buf, "OK") == 0){
			gdbclient_write(fd, "+", 1);
			break;    
		}
	}
	return 0;
}

static int palacios_gdb_attach_process(int pid, char *gdbcons_backend){
	return gdb_attach_process(pid, gdbcons_backend);
}

static int palacios_gdb_attach_thread(int pid, int tid, char *gdbcons_backend){
	return gdb_attach_thread(pid, tid, gdbcons_backend);
}

static unsigned long long palacios_gdb_read(void * file_ptr, void * buffer, unsigned long long len){
	struct file *fd = (struct file*) file_ptr;
	return gdbclient_read(fd, buffer, len);
}

static unsigned long long palacios_gdb_write(void * file_ptr, void * buffer, unsigned long long len) {
	struct file *fd = (struct file*) file_ptr;
	return gdbclient_write(fd, buffer, len);
}

static struct v3_gdb_hooks palacios_gdb_hooks = {
	.connect	= palacios_gdb_connect,
	.disconnect	= palacios_gdb_disconnect,

	.attach_process = palacios_gdb_attach_process,
	.attach_thread  = palacios_gdb_attach_thread,

	.write		= palacios_gdb_write,
	.read       = palacios_gdb_read,
};

int
palacios_gdb_init(void){
	V3_Init_GDB(&palacios_gdb_hooks);
	return 0;
}

DRIVER_INIT("module", palacios_gdb_init);


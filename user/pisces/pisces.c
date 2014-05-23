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

typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;


#include "palacios.h"
#include "pisces.h"
#include "xpmem-pisces.h"
#include <xpmem-ext.h>
#include <pthread.h>

char test_buf[4096]  __attribute__ ((aligned (512)));
char hello_buf[512]  __attribute__ ((aligned (512)));
char resp_buf[4066]  __attribute__ ((aligned (512)));

int block_layer_test(void) {
    int fd = 0;
    int ret = 0;

    memset(test_buf, 0, 4096);
    memset(hello_buf, 0, 512);
    memset(resp_buf, 0, 4096);

    sprintf(hello_buf, "Hello There\n");

    fd = open("/dev/block/sata-1", O_RDWR);
    printf("Opened SATA-0 %d\n", fd);

    ret = read(fd, test_buf, 4096);
    printf("Read %d bytes. Buf:\n", ret);
    printf("%s\n", test_buf);

    lseek(fd, 100 * 4096, 0);

    ret = write(fd, hello_buf, 512);
    printf("Write %d bytes. Buf:\n", ret);

    ret = read(fd, resp_buf, 512);
    printf("Read %d bytes. Resp1: %s\n", ret, resp_buf);

    lseek(fd, 100 * 4096, 0);

    read(fd, resp_buf + 1024, 512);
    printf("Read %d bytes. Resp2: %s\n", ret, resp_buf + 1024);

    close(fd);


    return 0;
}


int
cpu_mask_test(void) {
    int status, i, home_cpu, ncpus=0;
    cpu_set_t cpu_mask;

    printf("\n");
    printf("TEST BEGIN: Task Migration Test\n");

    home_cpu = sched_getcpu();
    printf("  My home CPU is: %d\n",  home_cpu);

    status = pthread_getaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
    if (status) {
	printf("    ERROR: pthread_getaffinity_np() status=%d\n", status);
	return -1;
    }

    printf("  My CPU mask is: ");
    for (i = 0; i < CPU_SETSIZE; i++) {
	if (CPU_ISSET(i, &cpu_mask)) {
	    printf("%d ", i);
	    ++ncpus;
	}
    }
    printf("\n");

    printf("  Total # CPUs:   %d\n", ncpus);

    return 0;
}

void * fn(void * arg) {
    int var = 12345;
    void * addr = (void *)((uintptr_t)&var & ~(PAGE_SIZE - 1));
    xpmem_segid_t segid;

    printf("addr: %p, var: %p, (offset: %d)\n", 
	addr,
	(void *)&var,
	(int)((void *)&var - addr));

    segid = xpmem_make(addr, 4096, XPMEM_PERMIT_MODE, NULL);
    printf("xpmem segid: %lli\n", (signed long long)segid);

    while (1) {}
    
    return NULL;
}

int
main(int argc, char ** argv, char * envp[]) {

    int pisces_fd = 0;
    struct pisces_cmd cmd;
    struct pisces_resp resp;

    memset(&cmd, 0, sizeof(struct pisces_cmd));
    memset(&resp, 0, sizeof(struct pisces_resp));

    printf("Pisces Control Daemon\n");
    // block_layer_test();

    pisces_fd = open("/pisces-cmd", O_RDWR);

    if (pisces_fd < 0) {
	printf("Error opening /pisces-cmd\n");
	return -1;
    }

    xpmem_pisces_add_pisces_dom();
    xpmem_pisces_add_local_dom();


    {
	pthread_t t;
	pthread_create(&t, NULL, fn, NULL);
    }

    while (1) {
	int ret = 0;

	/* Poll the ctrl and xpmem file descriptors */
	{
	    struct pollfd * poll_fds;
	    struct xpmem_dom * domain_list = NULL;
	    struct xpmem_dom * src_dom = NULL;
	    int list_size = 0;
	    int i = 0;

	    domain_list = xpmem_pisces_get_dom_list(&list_size);
	    if (!domain_list) {
		fprintf(stderr, "Could not get XPMEM domain list\n");
		return -1;
	    }

	    poll_fds = malloc(sizeof(struct pollfd) * (list_size + 1));
	    if (!poll_fds) {
		perror("malloc");
		return -1;
	    }

	    for (i = 1; i <= list_size; i++) {
		poll_fds[i].fd = domain_list[i - 1].enclave.fd;
		poll_fds[i].events = (POLLIN | POLLRDNORM);
	    }

	    poll_fds[0].fd = pisces_fd;
	    poll_fds[0].events = (POLLIN | POLLRDNORM);

	    if (poll(poll_fds, list_size + 1, -1) <= 0) {
		perror("poll");
		return -1;
	    }

	    if (!(poll_fds[0].revents & (POLLIN | POLLRDNORM))) {
		for (i = 1; i <= list_size; i++) {
		    if (poll_fds[i].revents & (POLLIN | POLLRDNORM)) {
			src_dom = &(domain_list[i - 1]);
			xpmem_pisces_xpmem_cmd(src_dom);
			break;
		    }
		}
	    }

	    free(poll_fds);
	    if (src_dom != NULL) {
		continue;
	    }
	}


	ret = read(pisces_fd, &cmd, sizeof(struct pisces_cmd));

	if (ret != sizeof(struct pisces_cmd)) {
	    printf("Error reading pisces CMD (ret=%d)\n", ret);
	    break;
	}

	//printf("Command=%llu, data_len=%d\n", cmd.cmd, cmd.data_len);

	switch (cmd.cmd) {
	    case ENCLAVE_CMD_ADD_MEM:
		{
		    struct cmd_mem_add mem_cmd;
		    struct pmem_region rgn;

		    memset(&mem_cmd, 0, sizeof(struct cmd_mem_add));
		    memset(&rgn, 0, sizeof(struct pmem_region));

		    ret = read(pisces_fd, &mem_cmd, sizeof(struct cmd_mem_add));

		    if (ret != sizeof(struct cmd_mem_add)) {
			printf("Error reading pisces MEM_ADD CMD (ret=%d)\n", ret);
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));

			break;
		    }


		    rgn.start = mem_cmd.phys_addr;
		    rgn.end = mem_cmd.phys_addr + mem_cmd.size;
		    rgn.type_is_set = 1;
		    rgn.type = PMEM_TYPE_UMEM;
		    rgn.allocated_is_set = 1;
		    rgn.allocated = 0;

		    printf("Adding pmem (%p - %p)\n", (void *)rgn.start, (void *)rgn.end);

		    ret = pmem_add(&rgn);

		    printf("pmem_add returned %d\n", ret);

		    ret = pmem_zero(&rgn);

		    printf("pmem_zero returned %d\n", ret);

		    resp.status = 0;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));

		    break;
		}
	    case ENCLAVE_CMD_ADD_CPU:
		{
		    struct cmd_cpu_add cpu_cmd;
		    int logical_cpu = 0;

		    ret = read(pisces_fd, &cpu_cmd, sizeof(struct cmd_cpu_add));

		    if (ret != sizeof(struct cmd_cpu_add)) {
			printf("Error reading pisces CPU_ADD CMD (ret=%d)\n", ret);
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));

			break;
		    }

		    printf("Adding CPU phys_id %llu, apic_id %llu\n", 
			    (unsigned long long) cpu_cmd.phys_cpu_id, 
			    (unsigned long long) cpu_cmd.apic_id);

		    logical_cpu = phys_cpu_add(cpu_cmd.phys_cpu_id, cpu_cmd.apic_id);

		    if (logical_cpu == -1) {
			printf("Error Adding CPU to Kitten\n");
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));

			break;
		    }

		    {
			printf("Adding CPU to Palacios\n");
			int palacios_fd = 0;

			palacios_fd = open("/palacios-cmd", O_RDWR);
			ret = ioctl(palacios_fd, V3_ADD_CPU, logical_cpu);

			if (ret < 0) {
			    printf("ERROR: Could not add CPU to Palacios\n");
			}

			close(palacios_fd);
		    }


		    resp.status = 0;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));

		    break;
		}
	    case ENCLAVE_CMD_REMOVE_CPU:
		{
		    struct cmd_cpu_add cpu_cmd;
		    int logical_cpu = 0;

		    ret = read(pisces_fd, &cpu_cmd, sizeof(struct cmd_cpu_add));

		    if (ret != sizeof(struct cmd_cpu_add)) {
			printf("Error reading pisces CPU_ADD CMD (ret=%d)\n", ret);
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));

			break;
		    }

		    printf("Removing CPU phys_id %llu, apic_id %llu\n", 
			    (unsigned long long) cpu_cmd.phys_cpu_id, 
			    (unsigned long long) cpu_cmd.apic_id);

		    logical_cpu = phys_cpu_remove(cpu_cmd.phys_cpu_id, cpu_cmd.apic_id);

		    if (logical_cpu == -1) {
			printf("Error remove CPU to Kitten\n");
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));

			break;
		    }

		    resp.status = 0;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));

		    break;
		}
	    case ENCLAVE_CMD_TEST_LCALL:
		{
		    ioctl(pisces_fd, 0, 0);
		    break;
		}

	    case ENCLAVE_CMD_CREATE_VM:
		{
		    struct cmd_create_vm vm_cmd;
		    int vm_id = -1;

		    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_create_vm));

		    if (ret != sizeof(struct cmd_create_vm)) {
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));
			break;
		    }


		    {
			printf("creating VM\n");
			int palacios_fd = 0;

			palacios_fd = open("/palacios-cmd", O_RDWR);
			vm_id = ioctl(palacios_fd, V3_CREATE_GUEST, &(vm_cmd.path));

			if (vm_id < 0) {
			    printf("ERROR: Could not create VM\n");
			}

			close(palacios_fd);
		    }

		    resp.status = vm_id;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));

		    break;
		}
	    case ENCLAVE_CMD_LAUNCH_VM:
		{
		    struct cmd_launch_vm vm_cmd;

		    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_launch_vm));

		    if (ret != sizeof(struct cmd_launch_vm)) {
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));
			break;
		    }


		    {

			int palacios_fd = 0;
			char vm_fname[128];

			memset(vm_fname, 0, 128);
			snprintf(vm_fname, 128, "/palacios-vm%d", vm_cmd.vm_id);

			printf("Launching VM %u\n", vm_cmd.vm_id);

			palacios_fd = open(vm_fname,  O_RDWR);

			if (ioctl(palacios_fd, V3_VM_LAUNCH, NULL) < 0) {
			    printf("ERROR: Could not Launch VM\n");
			}

			printf("VM has launched. returning\n");

			if (xpmem_pisces_add_dom(palacios_fd, vm_cmd.vm_id)) {
			    printf("ERROR: Could not add connect to Palacios VM %d XPMEM channel\n", vm_cmd.vm_id);
			}

			close(palacios_fd);
		    }


		    resp.status = 0;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));

		    break;
		}
	    case ENCLAVE_CMD_ADD_V3_PCI:
		{
		    struct cmd_add_pci_dev cmd;
		    struct cmd_add_pci_dev cmd_resp;
		    int ret = 0;

		    memset(&cmd, 0, sizeof(struct cmd_add_pci_dev));

		    printf("Adding V3 PCI Device (1)\n");

		    ret = read(pisces_fd, &cmd, sizeof(struct cmd_add_pci_dev));

		    if (ret != sizeof(struct cmd_add_pci_dev)) {
			cmd_resp.resp.status = -1;
			cmd_resp.resp.data_len = 0;

			write(pisces_fd, &cmd_resp, sizeof(struct cmd_add_pci_dev));
			break;
		    }

		    {
			printf("Adding PCI Device (2)\n");
			int palacios_fd = 0;

			palacios_fd = open("/palacios-cmd", O_RDWR);
			ret = ioctl(palacios_fd, V3_ADD_PCI_HW_DEV, &(cmd.device));


			close(palacios_fd);
		    }

		    if (ret < 0) {
			cmd_resp.resp.status = -1;
			cmd_resp.resp.data_len = 0;
		    } else {
			cmd_resp.resp.status = 0;
			cmd_resp.resp.data_len = sizeof(struct cmd_add_pci_dev)
			    - sizeof(struct pisces_resp);
			cmd_resp.device_ipi_vector = ret;
		    }

		    write(pisces_fd, &cmd_resp, sizeof(struct cmd_add_pci_dev));



		    break;
		}
	    case ENCLAVE_CMD_VM_CONS_CONNECT:
		{
		    struct cmd_vm_cons_connect vm_cmd;
		    u64 cons_ring_buf = 0;

		    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_cons_connect));

		    if (ret != sizeof(struct cmd_vm_cons_connect)) {
			resp.status = -1;
			resp.data_len = 0;

			printf("Error reading console command\n");

			write(pisces_fd, &resp, sizeof(struct pisces_resp));
			break;
		    }


		    {
			int palacios_fd = 0;
			char vm_fname[128];

			memset(vm_fname, 0, 128);
			snprintf(vm_fname, 128, "/palacios-vm%d", vm_cmd.vm_id);

			printf("Connecting to VM %u's console\n", vm_cmd.vm_id);

			palacios_fd = open(vm_fname,  O_RDWR);

			if (ioctl(palacios_fd, V3_VM_CONSOLE_CONNECT, &cons_ring_buf) < 0) {
			    printf("ERROR: Could connect to VM console\n");
			    cons_ring_buf = 0;
			}

			printf("Console ring buffer at %p\n", (void *)cons_ring_buf);

			close(palacios_fd);
		    }

		    printf("Cons Ring Buf=%p\n", (void *)cons_ring_buf);

		    resp.status = cons_ring_buf;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));

		    break;
		}

	    case ENCLAVE_CMD_VM_CONS_DISCONNECT:
		{
		    struct cmd_vm_cons_disconnect vm_cmd;

		    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_cons_disconnect));

		    if (ret != sizeof(struct cmd_vm_cons_disconnect)) {
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));
			break;
		    }


		    {
			int palacios_fd = 0;
			char vm_fname[128];

			memset(vm_fname, 0, 128);
			snprintf(vm_fname, 128, "/palacios-vm%d", vm_cmd.vm_id);

			printf("Disconnecting from VM %u's console\n", vm_cmd.vm_id);

			palacios_fd = open(vm_fname,  O_RDWR);

			if (ioctl(palacios_fd, V3_VM_CONSOLE_DISCONNECT, NULL) < 0) {
			    printf("ERROR: Could disconnect from VM console\n");
			}

			close(palacios_fd);
		    }

		    resp.status = 0;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));

		    break;
		}

	    case ENCLAVE_CMD_VM_CONS_KEYCODE:
		{
		    struct cmd_vm_cons_keycode vm_cmd;

		    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_cons_keycode));

		    if (ret != sizeof(struct cmd_vm_cons_keycode)) {
			resp.status = -1;
			resp.data_len = 0;

			write(pisces_fd, &resp, sizeof(struct pisces_resp));
			break;
		    }


		    {
			int palacios_fd = 0;
			char vm_fname[128];

			memset(vm_fname, 0, 128);
			snprintf(vm_fname, 128, "/palacios-vm%d", vm_cmd.vm_id);

			//printf("sending Keycode %x to VM %u's console\n", vm_cmd.scan_code, vm_cmd.vm_id);

			palacios_fd = open(vm_fname,  O_RDWR);

			if (ioctl(palacios_fd, V3_VM_KEYBOARD_EVENT, vm_cmd.scan_code) < 0) {
			    printf("ERROR: could not send keycode to VM console\n");
			}

			close(palacios_fd);
		    }

		    resp.status = 0;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));

		    break;
		}

	    default:
		{
		    resp.status = -1;
		    resp.data_len = 0;

		    write(pisces_fd, &resp, sizeof(struct pisces_resp));
		    break;
		}

	}
	//cpu_mask_test();
    }

    close(pisces_fd);

    return 0;
}


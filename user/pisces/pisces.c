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


#define PISCES_CMD_PATH "/pisces-cmd"


#include "palacios.h"
#include "pisces.h"

char test_buf[4096]  __attribute__ ((aligned (512)));
char hello_buf[512]  __attribute__ ((aligned (512)));
char resp_buf[4066]  __attribute__ ((aligned (512)));



static int 
send_resp(int fd, u64 err_code) 
{
	struct pisces_resp resp;

	resp.status   = err_code;
	resp.data_len = 0;
	
	write(fd, &resp, sizeof(struct pisces_resp));
	
	return 0;
}

static int 
issue_vm_cmd(int vm_id, u64 cmd, uintptr_t arg) 
{
	int palacios_fd = 0;
	char vm_fname[128];
	
	memset(vm_fname, 0, 128);
	snprintf(vm_fname, 128, V3_VM_PATH "%d", vm_id);
	
	palacios_fd = open(vm_fname,  O_RDWR);
	
	if (palacios_fd < 0) {
		printf("Could not open palacios VM file (%s)\n", vm_fname);
		return -1;
	}
	
	
	if (ioctl(palacios_fd, cmd,  arg) < 0) {
		printf("ERROR: Could not issue command (%llu) to guest (%d)\n", cmd, vm_id);
		return -1;
	}
	
	close(palacios_fd);
	
	return 0;
}

static int 
issue_v3_cmd(u64 cmd, uintptr_t arg) 
{
	int palacios_fd = 0;
	int ret         = 0;
	
	palacios_fd = open(V3_CMD_PATH, O_RDWR);
	
	if (palacios_fd < 0) {
		printf("Could not open palacios CMD file (%s)\n", V3_CMD_PATH);
		return -1;
	}
	
	ret = ioctl(palacios_fd, cmd, arg);

	
	close(palacios_fd);

	return ret;
}

int
main(int argc, char ** argv, char * envp[]) 
{
	struct pisces_cmd  cmd;

	int pisces_fd = 0;

	memset(&cmd,  0, sizeof(struct pisces_cmd));

	printf("Pisces Control Daemon\n");


	pisces_fd = open(PISCES_CMD_PATH, O_RDWR);

	if (pisces_fd < 0) {
		printf("Error opening pisces cmd file (%s)\n", PISCES_CMD_PATH);
		return -1;
	}

	while (1) {
		int ret = 0;

		ret = read(pisces_fd, &cmd, sizeof(struct pisces_cmd));

		if (ret != sizeof(struct pisces_cmd)) {
			printf("Error reading pisces CMD (ret=%d)\n", ret);
			break;
		}

		//printf("Command=%llu, data_len=%d\n", cmd.cmd, cmd.data_len);

		switch (cmd.cmd) {
		    case ENCLAVE_CMD_ADD_MEM: {
			    struct cmd_mem_add mem_cmd;
			    struct pmem_region rgn;

			    memset(&mem_cmd, 0, sizeof(struct cmd_mem_add));
			    memset(&rgn, 0, sizeof(struct pmem_region));

			    ret = read(pisces_fd, &mem_cmd, sizeof(struct cmd_mem_add));

			    if (ret != sizeof(struct cmd_mem_add)) {
				    printf("Error reading pisces MEM_ADD CMD (ret=%d)\n", ret);
				    send_resp(pisces_fd, -1);
				    break;
			    }


			    rgn.start            = mem_cmd.phys_addr;
			    rgn.end              = mem_cmd.phys_addr + mem_cmd.size;
			    rgn.type_is_set      = 1;
			    rgn.type             = PMEM_TYPE_UMEM;
			    rgn.allocated_is_set = 1;
			    rgn.allocated        = 0;

			    printf("Adding pmem (%p - %p)\n", (void *)rgn.start, (void *)rgn.end);

			    ret = pmem_add(&rgn);

			    printf("pmem_add returned %d\n", ret);

			    ret = pmem_zero(&rgn);

			    printf("pmem_zero returned %d\n", ret);

			    send_resp(pisces_fd, 0);

			    break;
		    }
		    case ENCLAVE_CMD_ADD_CPU: {
			    struct cmd_cpu_add cpu_cmd;
			    int logical_cpu = 0;

			    ret = read(pisces_fd, &cpu_cmd, sizeof(struct cmd_cpu_add));

			    if (ret != sizeof(struct cmd_cpu_add)) {
				    printf("Error reading pisces CPU_ADD CMD (ret=%d)\n", ret);

				    send_resp(pisces_fd, -1);
				    break;
			    }

			    printf("Adding CPU phys_id %llu, apic_id %llu\n", 
				   (unsigned long long) cpu_cmd.phys_cpu_id, 
				   (unsigned long long) cpu_cmd.apic_id);

			    logical_cpu = phys_cpu_add(cpu_cmd.phys_cpu_id, cpu_cmd.apic_id);

			    if (logical_cpu == -1) {
				    printf("Error Adding CPU to Kitten\n");
				    send_resp(pisces_fd, -1);

				    break;
			    }

			    /* Notify Palacios of New CPU */
			    if (issue_v3_cmd(V3_ADD_CPU, (uintptr_t)logical_cpu) == -1) {
				    printf("Error: Could not add CPU to Palacios\n");
			    }

			    send_resp(pisces_fd, 0);
			    break;
		    }
		    case ENCLAVE_CMD_REMOVE_CPU: {
			    struct cmd_cpu_add cpu_cmd;
			    int logical_cpu = 0;

			    ret = read(pisces_fd, &cpu_cmd, sizeof(struct cmd_cpu_add));

			    if (ret != sizeof(struct cmd_cpu_add)) {
				    printf("Error reading pisces CPU_ADD CMD (ret=%d)\n", ret);

				    send_resp(pisces_fd, -1);
				    break;
			    }

			    printf("Removing CPU phys_id %llu, apic_id %llu\n", 
				   (unsigned long long) cpu_cmd.phys_cpu_id, 
				   (unsigned long long) cpu_cmd.apic_id);

			    logical_cpu = phys_cpu_remove(cpu_cmd.phys_cpu_id, cpu_cmd.apic_id);

			    if (logical_cpu == -1) {
				    printf("Error remove CPU to Kitten\n");

				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);
			    break;
		    }

		    case ENCLAVE_CMD_CREATE_VM: {
			    struct cmd_create_vm vm_cmd;
			    int vm_id = -1;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_create_vm));

			    if (ret != sizeof(struct cmd_create_vm)) {
				    send_resp(pisces_fd, -1);
				    printf("Error: CREATE_VM command could not be read\n");
				    break;
			    }

			    
			    /* Issue VM Create command to Palacios */
			    vm_id = issue_v3_cmd(V3_CREATE_GUEST, (uintptr_t)&(vm_cmd.path));

			    if (vm_id < 0) {
				    printf("Error: Could not create VM (%s) at (%s) (err=%d)\n", 
					   vm_cmd.path.vm_name, vm_cmd.path.file_name, vm_id);
				    send_resp(pisces_fd, vm_id);
				    break;
			    }

			    printf("Created VM (%d)\n", vm_id);

			    send_resp(pisces_fd, vm_id);
			    break;
		    }
		    case ENCLAVE_CMD_FREE_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_v3_cmd(V3_FREE_GUEST, (uintptr_t)vm_cmd.vm_id) == -1) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);

			    break;
		    }
		    case ENCLAVE_CMD_ADD_V3_PCI: {
			    struct cmd_add_pci_dev cmd;
			    struct v3_hw_pci_dev   v3_pci_spec;
			    int ret = 0;

			    memset(&cmd, 0, sizeof(struct cmd_add_pci_dev));

			    printf("Adding V3 PCI Device\n");

			    ret = read(pisces_fd, &cmd, sizeof(struct cmd_add_pci_dev));

			    if (ret != sizeof(struct cmd_add_pci_dev)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    memcpy(v3_pci_spec.name, cmd.spec.name, 128);
			    v3_pci_spec.bus  = cmd.spec.bus;
			    v3_pci_spec.dev  = cmd.spec.dev;
			    v3_pci_spec.func = cmd.spec.func;


			    /* Issue Device Add operation to Palacios */
			    if (issue_v3_cmd(V3_ADD_PCI, (uintptr_t)&(v3_pci_spec)) == -1) {
				    printf("Error: Could not add PCI device to Palacios\n");
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);
			    break;
		    }
		    case ENCLAVE_CMD_FREE_V3_PCI: {
			    struct cmd_add_pci_dev cmd;
			    struct v3_hw_pci_dev   v3_pci_spec;
			    int ret = 0;

			    memset(&cmd, 0, sizeof(struct cmd_add_pci_dev));

			    printf("Removing V3 PCI Device\n");

			    ret = read(pisces_fd, &cmd, sizeof(struct cmd_add_pci_dev));

			    if (ret != sizeof(struct cmd_add_pci_dev)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    memcpy(v3_pci_spec.name, cmd.spec.name, 128);
			    v3_pci_spec.bus  = cmd.spec.bus;
			    v3_pci_spec.dev  = cmd.spec.dev;
			    v3_pci_spec.func = cmd.spec.func;


			    /* Issue Device Add operation to Palacios */
			    if (issue_v3_cmd(V3_REMOVE_PCI, (uintptr_t)&(v3_pci_spec)) == -1) {
				    printf("Error: Could not remove PCI device from Palacios\n");
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);
			    break;
		    }
		    case ENCLAVE_CMD_LAUNCH_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_LAUNCH, (uintptr_t)NULL) == -1) {
				    send_resp(pisces_fd, -1);
				    break;
			    }


			    /*
			      if (xpmem_pisces_add_dom(palacios_fd, vm_cmd.vm_id)) {
			      printf("ERROR: Could not add connect to Palacios VM %d XPMEM channel\n", 
			      vm_cmd.vm_id);
			      }
			    */

			    send_resp(pisces_fd, 0);

			    break;
		    }
		    case ENCLAVE_CMD_STOP_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_STOP, (uintptr_t)NULL) == -1) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);

			    break;
		    }

		    case ENCLAVE_CMD_PAUSE_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_PAUSE, (uintptr_t)NULL) == -1) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);

			    break;
		    }
		    case ENCLAVE_CMD_CONTINUE_VM: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to Launch VM */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_CONTINUE, (uintptr_t)NULL) == -1) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);

			    break;
		    }
		    case ENCLAVE_CMD_VM_CONS_CONNECT: {
			    struct cmd_vm_ctrl vm_cmd;
			    u64 cons_ring_buf = 0;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    printf("Error reading console command\n");

				    send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Signal Palacios to connect the console */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_CONSOLE_CONNECT, (uintptr_t)&cons_ring_buf) == -1) {
				    cons_ring_buf        = 0;
			    }
					

			    printf("Cons Ring Buf=%p\n", (void *)cons_ring_buf);
			    send_resp(pisces_fd, cons_ring_buf);

			    break;
		    }

		    case ENCLAVE_CMD_VM_CONS_DISCONNECT: {
			    struct cmd_vm_ctrl vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

			    if (ret != sizeof(struct cmd_vm_ctrl)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }


			    /* Send Disconnect Request to Palacios */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_CONSOLE_DISCONNECT, (uintptr_t)NULL) == -1) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);
			    break;
		    }

		    case ENCLAVE_CMD_VM_CONS_KEYCODE: {
			    struct cmd_vm_cons_keycode vm_cmd;

			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_cons_keycode));

			    if (ret != sizeof(struct cmd_vm_cons_keycode)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    /* Send Keycode to Palacios */
			    if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_KEYBOARD_EVENT, vm_cmd.scan_code) == -1) {
				    send_resp(pisces_fd, -1);
				    break;
			    }

			    send_resp(pisces_fd, 0);
			    break;
		    }

		    case ENCLAVE_CMD_VM_DBG: {
			    struct cmd_vm_debug pisces_cmd;
			    struct v3_debug_cmd v3_cmd;
			    
			    ret = read(pisces_fd, &pisces_cmd, sizeof(struct cmd_vm_debug));
			    
			    if (ret != sizeof(struct cmd_vm_debug)) {
				    send_resp(pisces_fd, -1);
				    break;
			    }
			    
			    v3_cmd.core = pisces_cmd.core;
			    v3_cmd.cmd  = pisces_cmd.cmd;
			    
			    if (issue_vm_cmd(pisces_cmd.vm_id, V3_VM_DEBUG, (uintptr_t)&v3_cmd) == -1) {
				    send_resp(pisces_fd, -1);
				    break;
			    }
			    
			    send_resp(pisces_fd, 0);
			    break;
		    }

		    case ENCLAVE_CMD_SHUTDOWN: {

			if (issue_v3_cmd(V3_SHUTDOWN, 0) == -1) {
			    printf("Error: Could not shutdown Palacios VMM\n");
			    send_resp(pisces_fd, -1);
			    break;
			}
			
			/* Perform additional Cleanup is necessary */

			send_resp(pisces_fd, 0);

			close(pisces_fd);
			exit(0);

		    }
		    default: {
			    printf("Unknown Pisces Command (%llu)\n", cmd.cmd);
			    send_resp(pisces_fd, -1);
			    break;
		    }

		}
	}

	close(pisces_fd);

	return 0;
}

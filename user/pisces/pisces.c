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

typedef signed   char s8;
typedef unsigned char u8;

typedef signed   short s16;
typedef unsigned short u16;

typedef signed   int s32;
typedef unsigned int u32;

typedef signed   long long s64;
typedef unsigned long long u64;


#define PISCES_CMD_PATH "/dev/pisces"


#include "palacios.h"
#include "pisces.h"

char test_buf[4096]  __attribute__ ((aligned (512)));
char hello_buf[512]  __attribute__ ((aligned (512)));
char resp_buf[4066]  __attribute__ ((aligned (512)));

cpu_set_t   enclave_cpus;

struct v3_guest_img {
    unsigned long long   size;
    void               * guest_data;
    char                 name[128];
} __attribute__((packed));

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


static int
load_file(int pisces_fd, 
	  char * lnx_file, 
	  char * lwk_file)
{
	struct pisces_user_file_info * file_info = NULL;
	int    path_len  = strlen(lnx_file) + 1;
	size_t file_size = 0;
	char * file_buf  = NULL;

	file_info = malloc(sizeof(struct pisces_user_file_info) + path_len);
	memset(file_info, 0, sizeof(struct pisces_user_file_info) + path_len);
    
	file_info->path_len = path_len;
	strncpy(file_info->path, lnx_file, path_len - 1);
    
	file_size = ioctl(pisces_fd, PISCES_STAT_FILE, file_info);
    

	file_buf = (char *)malloc(file_size);

	if (!file_buf) {
	    printf("Error: Could not allocate space for file (%s)\n", lnx_file);
	    return -1;
	}

	file_info->user_addr = (uintptr_t)file_buf;
	ioctl(pisces_fd, PISCES_LOAD_FILE, file_info);

	{
	    FILE * new_file = fopen(lwk_file, "w+");
	    
	    fwrite(file_buf, file_size, 1, new_file);

	    fclose(new_file);
	}

	free(file_buf);

	return 0;
}


static int 
launch_job(int pisces_fd, struct pisces_job_spec * job_spec)
{

    u32 page_size = (job_spec->use_large_pages ? VM_PAGE_2MB : VM_PAGE_4KB);

    vaddr_t   file_addr = 0;
    cpu_set_t spec_cpus;
    cpu_set_t job_cpus;
    user_cpumask_t lwk_cpumask;

    int status = 0;


    /* Figure out which CPUs are being requested */
    {
	int i = 0;

	CPU_ZERO(&spec_cpus);
	
	for (i = 0; i < 64; i++) {
	    if ((job_spec->cpu_mask & (0x1ULL << i)) != 0) {
		CPU_SET(i, &spec_cpus);
	    }
	}
    }




    /* Check if we can host the job on the current CPUs */
    /* Create a kitten compatible cpumask */
    {
	int i = 0;

	CPU_AND(&job_cpus, &spec_cpus, &enclave_cpus);

	if (CPU_COUNT(&job_cpus) < job_spec->num_ranks) {
	    printf("Error: Could not find enough CPUs for job\n");
	    return -1;
	}
	

	cpus_clear(lwk_cpumask);
	
	for (i = 0; (i < CPU_MAX_ID) && (i < CPU_SETSIZE); i++) {
	    if (CPU_ISSET(i, &job_cpus)) {
		cpu_set(i, lwk_cpumask);
	    }
	}



    }


    /* Load exe file info memory */
    {
	struct pisces_user_file_info * file_info = NULL;
	int    path_len  = strlen(job_spec->exe_path) + 1;
	size_t file_size = 0;
	id_t   my_aspace_id;

	file_info = malloc(sizeof(struct pisces_user_file_info) + path_len);
	memset(file_info, 0, sizeof(struct pisces_user_file_info) + path_len);
    
	file_info->path_len = path_len;
	strncpy(file_info->path, job_spec->exe_path, path_len - 1);
    
	file_size = ioctl(pisces_fd, PISCES_STAT_FILE, file_info);
    
	status = aspace_get_myid(&my_aspace_id);
	if (status != 0) 
	    return status;


	{
	
	    paddr_t pmem = elf_dflt_alloc_pmem(file_size, page_size, 0);

	    printf("PMEM Allocated at %p (file_size=%lu) (page_size=0x%x) (pmem_size=%p)\n", 
		   (void *)pmem, 
		   file_size,
		   page_size, 
		   (void *)round_up(file_size, page_size));

	    if (pmem == 0) {
		printf("Could not allocate space for exe file\n");
		return -1;
	    }

	    /* Map the segment into this address space */
	    status =
		aspace_map_region_anywhere(
					   my_aspace_id,
					   &file_addr,
					   round_up(file_size, page_size),
					   (VM_USER|VM_READ|VM_WRITE),
					   page_size,
					   "File",
					   pmem
					   );
	    if (status)
		return status;
	

	    file_info->user_addr = file_addr;
	}
    
	printf("Loading EXE into memory\n");

	ioctl(pisces_fd, PISCES_LOAD_FILE, file_info);


	free(file_info);
    }


    printf("Job Launch Request (%s) [%s %s]\n", job_spec->name, job_spec->exe_path, job_spec->argv);


    /* Initialize start state for each rank */
    {
	start_state_t * start_state = NULL;
	int rank = 0; 

	/* Allocate start state for each rank */
	start_state = malloc(job_spec->num_ranks * sizeof(start_state_t));
	if (!start_state) {
	    printf("malloc of start_state[] failed\n");
	    return -1;
	}


	for (rank = 0; rank < job_spec->num_ranks; rank++) {
	    int cpu = 0;
	    int i   = 0;
	    
	    for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &job_cpus)) {
		    CPU_CLR(i, &job_cpus);
		    cpu = i;
		    break;
		}
	    }


	    printf("Loading Rank %d on CPU %d\n", rank, cpu);
	    
	    start_state[rank].task_id  = ANY_ID;
	    start_state[rank].cpu_id   = ANY_ID;
	    start_state[rank].user_id  = 1;
	    start_state[rank].group_id = 1;
	    
	    sprintf(start_state[rank].task_name, job_spec->name);


	    status = elf_load((void *)file_addr,
			      job_spec->name,
			      ANY_ID,
			      page_size, 
			      job_spec->heap_size,   // heap_size 
			      job_spec->stack_size,  // stack_size 
			      job_spec->argv,        // argv_str
			      job_spec->envp,        // envp_str
			      &start_state[rank],
			      0,
			      &elf_dflt_alloc_pmem
			      );
	    

	    if (status) {
		printf("elf_load failed, status=%d\n", status);
	    }
	    

	    if ( aspace_update_user_cpumask(start_state[rank].aspace_id, &lwk_cpumask) != 0) {
		printf("Error updating CPU mask\n");
		return -1;
	    }
		 

	    /* Setup Smartmap regions if enabled */
	    if (job_spec->use_smartmap) {
		int src = 0;
		int dst = 0;

		printf("Creating SMARTMAP mappings...\n");
		for (dst = 0; dst < job_spec->num_ranks; dst++) {
		    for (src = 0; src < job_spec->num_ranks; src++) {
			status =
			    aspace_smartmap(
					    start_state[src].aspace_id,
					    start_state[dst].aspace_id,
					    SMARTMAP_ALIGN + (SMARTMAP_ALIGN * src),
					    SMARTMAP_ALIGN
					    );
			
			if (status) {
			    printf("smartmap failed, status=%d\n", status);
			    return -1;
			}
		    }
		}
		printf("    OK\n");
	    }

	    
	    printf("Creating Task\n");
	    
	    status = task_create(&start_state[rank], NULL);
	}
    }
    

    
    return 0;
}
    
	
int
main(int argc, char ** argv, char * envp[]) 
{
	struct pisces_cmd  cmd;

	int pisces_fd = 0;

	memset(&cmd,  0, sizeof(struct pisces_cmd));

	printf("Pisces Control Daemon\n");


	CPU_ZERO(&enclave_cpus);	/* Initialize CPU mask */
	CPU_SET(0, &enclave_cpus);      /* We always boot on CPU 0 */


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
			    
			    CPU_SET(logical_cpu, &enclave_cpus);

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

			    CPU_CLR(logical_cpu, &enclave_cpus);

			    send_resp(pisces_fd, 0);
			    break;
		    }

		    case ENCLAVE_CMD_LAUNCH_JOB: {
			struct cmd_launch_job * job_cmd = malloc(sizeof(struct cmd_launch_job));
			int ret = 0;

			memset(job_cmd, 0, sizeof(struct cmd_launch_job));

			ret = read(pisces_fd, job_cmd, sizeof(struct cmd_launch_job));

			if (ret != sizeof(struct cmd_launch_job)) {
			    printf("Error reading Job Launch CMD (ret = %d)\n", ret);

			    free(job_cmd);
			    
			    send_resp(pisces_fd, -1);
			    break;
			}
			
			ret = launch_job(pisces_fd, &(job_cmd->spec));

			free(job_cmd);
			
			send_resp(pisces_fd, ret);
			break;
		    }
		    case ENCLAVE_CMD_LOAD_FILE: {
			struct cmd_load_file * load_cmd = malloc(sizeof(struct cmd_load_file));
			int ret = 0;

			memset(load_cmd, 0, sizeof(struct cmd_load_file));

			ret = read(pisces_fd, load_cmd, sizeof(struct cmd_load_file));

			if (ret != sizeof(struct cmd_load_file)) {
			    printf("Error reading LOAD FILE CMD (ret = %d)\n", ret);

			    free(load_cmd);
			    
			    send_resp(pisces_fd, -1);
			    break;
			}
			
			ret = load_file(pisces_fd, load_cmd->file_pair.lnx_file, load_cmd->file_pair.lwk_file);

			free(load_cmd);

			send_resp(pisces_fd, ret);

			break;
		    } 
		    case ENCLAVE_CMD_STORE_FILE: {
			
			
			break;
		    } 

			
		    case ENCLAVE_CMD_CREATE_VM: {
			    struct pisces_user_file_info * file_info = NULL;
			    struct cmd_create_vm vm_cmd;
			    struct pmem_region rgn;
			    struct v3_guest_img guest_img;

			    id_t    my_aspace_id;
			    vaddr_t file_addr;
			    size_t  file_size =  0;
			    int     path_len  =  0;
			    int     vm_id     = -1;
			    int     status    =  0;


			    memset(&vm_cmd,    0, sizeof(struct cmd_create_vm));
			    memset(&rgn,       0, sizeof(struct pmem_region));
			    memset(&guest_img, 0, sizeof(struct v3_guest_img));
			    
			    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_create_vm));

			    if (ret != sizeof(struct cmd_create_vm)) {
				    send_resp(pisces_fd, -1);
				    printf("Error: CREATE_VM command could not be read\n");
				    break;
			    }


			    path_len = strlen((char *)vm_cmd.path.file_name) + 1;

			    file_info = malloc(sizeof(struct pisces_user_file_info) + path_len);
			    memset(file_info, 0, sizeof(struct pisces_user_file_info) + path_len);

			    file_info->path_len = path_len;
			    strncpy(file_info->path, (char *)vm_cmd.path.file_name, path_len - 1);
			    
			    file_size = ioctl(pisces_fd, PISCES_STAT_FILE, file_info);

		
			    status = aspace_get_myid(&my_aspace_id);
			    if (status != 0) 
				return status;

			    if (pmem_alloc_umem(file_size, PAGE_SIZE, &rgn)) {
				printf("Error: Could not allocate umem for guest image (size=%lu)\n", file_size);
				break;
			    }
			    pmem_zero(&rgn);
				
			    status =
				aspace_map_region_anywhere(
							   my_aspace_id,
							   &file_addr,
							   round_up(file_size, PAGE_SIZE),
							   (VM_USER|VM_READ|VM_WRITE),
							   PAGE_SIZE,
							   "VM Image",
							   rgn.start
							   );


			    file_info->user_addr = file_addr;
		
			    ioctl(pisces_fd, PISCES_LOAD_FILE, file_info);
				
			    guest_img.size       = file_size;
			    guest_img.guest_data = (void *)file_info->user_addr;
			    strncpy(guest_img.name, (char *)vm_cmd.path.vm_name, 127);
				
				
			    /* Issue VM Create command to Palacios */
			    vm_id = issue_v3_cmd(V3_CREATE_GUEST, (uintptr_t)&guest_img);
				
				
			    aspace_unmap_region(my_aspace_id, file_addr, round_up(file_size, PAGE_SIZE));
			    pmem_free_umem(&rgn);
		

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
			    
			    v3_cmd.core = pisces_cmd.spec.core;
			    v3_cmd.cmd  = pisces_cmd.spec.cmd;
			    
			    if (issue_vm_cmd(pisces_cmd.spec.vm_id, V3_VM_DEBUG, (uintptr_t)&v3_cmd) == -1) {
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

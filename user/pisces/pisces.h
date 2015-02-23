
#define ENCLAVE_CMD_ADD_CPU            100
#define ENCLAVE_CMD_ADD_MEM            101


#define ENCLAVE_CMD_REMOVE_CPU         110
#define ENCLAVE_CMD_REMOVE_MEM         111


#define ENCLAVE_CMD_CREATE_VM          120
#define ENCLAVE_CMD_FREE_VM            121
#define ENCLAVE_CMD_LAUNCH_VM          122
#define ENCLAVE_CMD_STOP_VM            123
#define ENCLAVE_CMD_PAUSE_VM           124
#define ENCLAVE_CMD_CONTINUE_VM        125
#define ENCLAVE_CMD_SIMULATE_VM        126

#define ENCLAVE_CMD_VM_MOVE_CORE       140
#define ENCLAVE_CMD_VM_DBG             141


#define ENCLAVE_CMD_VM_CONS_CONNECT    150
#define ENCLAVE_CMD_VM_CONS_DISCONNECT 151
#define ENCLAVE_CMD_VM_CONS_KEYCODE    152

#define ENCLAVE_CMD_ADD_V3_PCI         180
#define ENCLAVE_CMD_ADD_V3_SATA        181

#define ENCLAVE_CMD_FREE_V3_PCI        190

#define ENCLAVE_CMD_LAUNCH_JOB         200
#define ENCLAVE_CMD_LOAD_FILE          201
#define ENCLAVE_CMD_STORE_FILE         202

#define ENCLAVE_CMD_XPMEM_CMD_EX       300

#define ENCLAVE_CMD_SHUTDOWN           900



struct pisces_pci_spec {
    u8 name[128];
    u32 bus;
    u32 dev;
    u32 func;
} __attribute__((packed));


struct pisces_job_spec {
    char name[64];
    char exe_path[256];
    char argv[256];
    char envp[256];

    union {
	u64 flags;
	struct {
	    u64   use_large_pages : 1;
	    u64   use_smartmap    : 1;
	    u64   rsvd            : 62;
	} __attribute__((packed));
    } __attribute__((packed));

    u8   num_ranks;
    u64  cpu_mask;
    u64  heap_size;
    u64  stack_size;
} __attribute__((packed));


struct pisces_file_pair {
    char lnx_file[128];
    char lwk_file[128];
} __attribute__((packed));



struct pisces_dbg_spec {
    u32 vm_id;
    u32 core;
    u32 cmd;
} __attribute__((packed));





struct pisces_cmd {
    u64 cmd;
    u32 data_len;
    u8  data[0];
} __attribute__((packed));


struct pisces_resp {
    u64 status;
    u32 data_len;
    u8  data[0];
} __attribute__((packed));


/* Linux -> Enclave Commands */
struct cmd_cpu_add {
    struct pisces_cmd hdr;
    u64 phys_cpu_id;
    u64 apic_id;
} __attribute__((packed));


struct cmd_mem_add {
    struct pisces_cmd hdr;
    u64 phys_addr;
    u64 size;
} __attribute__((packed));


struct vm_path {
    uint8_t file_name[256];
    uint8_t vm_name[128];
} __attribute__((packed));

struct cmd_create_vm {
    struct pisces_cmd hdr;
    struct vm_path    path;
} __attribute__((packed));



struct cmd_vm_ctrl {
    struct pisces_cmd hdr;

    u32 vm_id;
} __attribute__((packed));


struct cmd_vm_cons_keycode {
    struct pisces_cmd hdr;

    u32 vm_id;
    u8  scan_code;
} __attribute__((packed));



struct cmd_vm_debug {
    struct pisces_cmd hdr;
    struct pisces_dbg_spec spec;
} __attribute__((packed));




struct cmd_add_pci_dev {
    struct pisces_cmd hdr;

    struct pisces_pci_spec spec;
} __attribute__((packed));



struct cmd_free_pci_dev {
    struct pisces_cmd hdr;

    struct pisces_pci_spec spec;
} __attribute__((packed));




struct cmd_launch_job {
    struct pisces_cmd      hdr;
    struct pisces_job_spec spec;
} __attribute__((packed));



struct cmd_load_file {
    struct pisces_cmd       hdr;
    struct pisces_file_pair file_pair;
} __attribute__((packed));



/*** Local Kitten Requests for Pisces ***/

#define PISCES_STAT_FILE   500
#define PISCES_LOAD_FILE   501
#define PISCES_WRITE_FILE  502


struct pisces_user_file_info {
    unsigned long long user_addr;
    unsigned int       path_len;
    char               path[0];
} __attribute__((packed)); 

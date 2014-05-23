
#define ENCLAVE_CMD_ADD_CPU 100
#define ENCLAVE_CMD_ADD_MEM 101
#define ENCLAVE_CMD_TEST_LCALL 102
#define ENCLAVE_CMD_REMOVE_CPU 103
#define ENCLAVE_CMD_CREATE_VM 120
#define ENCLAVE_CMD_LAUNCH_VM 121
#define ENCLAVE_CMD_VM_CONS_CONNECT 122
#define ENCLAVE_CMD_VM_CONS_DISCONNECT 123
#define ENCLAVE_CMD_VM_CONS_KEYCODE 124
#define ENCLAVE_CMD_ADD_V3_PCI 125


struct pisces_cmd {
    u64 cmd;
    u32 data_len;
    u8 data[0];
} __attribute__((packed));


struct pisces_resp {
    u64 status;
    u32 data_len;
    u8 data[0];
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
    struct vm_path path;
} __attribute__((packed));

struct cmd_launch_vm {
    struct pisces_cmd hdr;
    u32 vm_id;
} __attribute__((packed));

struct cmd_vm_cons_connect {
    struct pisces_cmd hdr;
    u32 vm_id;
} __attribute__((packed));


struct cmd_vm_cons_disconnect {
    struct pisces_cmd hdr;
    u32 vm_id;
} __attribute__((packed));


struct cmd_vm_cons_keycode {
    struct pisces_cmd hdr;
    u32 vm_id;
    u8 scan_code;
} __attribute__((packed));


struct pisces_pci_dev {
    u8 name[128];
    u32 bus;
    u32 dev;
    u32 func;
} __attribute__((packed));


struct cmd_add_pci_dev {
    union {
        struct pisces_cmd hdr;
        struct pisces_resp resp;
    } __attribute__((packed));
    struct pisces_pci_dev device;
    u32 device_ipi_vector;
} __attribute__((packed));

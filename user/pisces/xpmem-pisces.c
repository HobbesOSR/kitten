#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <lwk/liblwk.h>
#include <lwk/types.h>

#include <xpmem-ext.h>
#include <arch/pisces/pisces_xpmem.h>
#include "palacios.h"

/* List of attached XPMEM-EXT domains */
static struct xpmem_dom * pisces_dom = NULL;
static struct xpmem_dom * xpmem_domain_list = NULL;
static int list_size = 0;

static char * cmd_to_string(xpmem_op_t op) {
    switch (op) {
        case XPMEM_MAKE:
            return "XPMEM_MAKE";
        case XPMEM_REMOVE:
            return "XPMEM_REMOVE";
        case XPMEM_GET:
            return "XPMEM_GET";
        case XPMEM_RELEASE:
            return "XPMEM_RELEASE";
        case XPMEM_ATTACH:
            return "XPMEM_ATTACH";
        case XPMEM_DETACH:
            return "XPMEM_DETACH";
        case XPMEM_MAKE_COMPLETE:
            return "XPMEM_MAKE_COMPLETE";
        case XPMEM_REMOVE_COMPLETE:
            return "XPMEM_REMOVE_COMPLETE";
        case XPMEM_GET_COMPLETE:
            return "XPMEM_GET_COMPLETE";
        case XPMEM_RELEASE_COMPLETE:
            return "XPMEM_RELEASE_COMPLETE";
        case XPMEM_ATTACH_COMPLETE:
            return "XPMEM_ATTACH_COMPLETE";
        case XPMEM_DETACH_COMPLETE:
            return "XPMEM_DETACH_COMPLETE";
	default:
	    return "UNKNOWN OPERATION";
    }
}

static inline char * dom_type_to_string(xpmem_endpoint_t p) {
    switch (p) {
        case VM:
            return "VM";
        case ENCLAVE:
            return "Enclave";
        case LOCAL:
            return "Process";
        default:
            return "Unknown";
    }
}

static void set_src_dom(struct xpmem_cmd_ex * cmd, struct xpmem_dom * dom) {
    switch(cmd->type) {
        case XPMEM_MAKE:
        case XPMEM_REMOVE:
        case XPMEM_GET:
        case XPMEM_RELEASE:
        case XPMEM_ATTACH:
        case XPMEM_DETACH:
	    if (dom != pisces_dom) {
		cmd->src_dom.enclave = dom->enclave;
	    }
            break;

        default:
            break;
    }
}

static void set_dst_dom(struct xpmem_cmd_ex * cmd, struct xpmem_dom * dom) {
    cmd->dst_dom = *dom;
}

static void print_cmd_route(struct xpmem_cmd_ex * cmd) {
    printf("cmd: %s\n"
        "  src: %s %d (fd: %d)\n"
        "  dst: %s %d (fd: %d)\n",
        cmd_to_string(cmd->type),
        dom_type_to_string(cmd->src_dom.enclave.type), cmd->src_dom.enclave.id, cmd->src_dom.enclave.fd,
        dom_type_to_string(cmd->dst_dom.enclave.type), cmd->dst_dom.enclave.id, cmd->dst_dom.enclave.fd
    );
}


int xpmem_pisces_xpmem_cmd(struct xpmem_dom * ready_dom) {
    int bytes = 0;
    int xpmem_fd = 0;
    struct xpmem_cmd_ex cmd;

    xpmem_fd = ready_dom->enclave.fd;
    if ((bytes = read(xpmem_fd, &cmd, sizeof(struct xpmem_cmd_ex))) != sizeof(struct xpmem_cmd_ex)) {
	fprintf(stderr, "Could not read XPMEM cmd (read %d bytes)\n", bytes);
	return -1;
    }

    set_src_dom(&cmd, ready_dom);

    switch (cmd.type) {
	case XPMEM_MAKE:
	case XPMEM_REMOVE:
	    if (ready_dom != pisces_dom) {
		set_dst_dom(&cmd, pisces_dom);
	    } else {
		fprintf(stderr, "Unhandled XPMEM command from Pisces: %d\n", cmd.type);
		return -1;
	    }

	    break;

	case XPMEM_GET:
	case XPMEM_RELEASE:
	case XPMEM_ATTACH:
	case XPMEM_DETACH: {
	    if (ready_dom != pisces_dom) {
		set_dst_dom(&cmd, pisces_dom);
	    } else {
		if (cmd.dst_dom.enclave.type == LOCAL) {
		    set_dst_dom(&cmd, &(xpmem_domain_list[2]));
		}
	    }

	    break;
	}

	case XPMEM_MAKE_COMPLETE:
	case XPMEM_REMOVE_COMPLETE:
	    if (ready_dom != pisces_dom) {
		fprintf(stderr, "Unhandled XPMEM command from Kitten process: %s\n", cmd_to_string(cmd.type));
		return -1;
	    }

	    break;

	case XPMEM_GET_COMPLETE:
	case XPMEM_RELEASE_COMPLETE:
	case XPMEM_ATTACH_COMPLETE:
	case XPMEM_DETACH_COMPLETE: 
	    if (ready_dom != pisces_dom) {
		set_dst_dom(&cmd, pisces_dom);
	    } 

	    break;

	default:
	    fprintf(stderr, "Unhandled XPMEM command: %d\n", cmd.type);
	    return -1;
    }

    print_cmd_route(&cmd);

    xpmem_fd = cmd.dst_dom.enclave.fd;
    if (write(xpmem_fd, (void *)&cmd, sizeof(struct xpmem_cmd_ex)) != sizeof(struct xpmem_cmd_ex)) {
	fprintf(stderr, "Could not write XPMEM cmd\n");
	return -1;
    }

    return 0;
}


#define BUF_SIZE 64
int xpmem_pisces_add_pisces_dom(void) {
    int xpmem_fd = 0;

    xpmem_fd = open("/pisces-xpmem", O_RDWR);
    if (xpmem_fd <= 0) {
	fprintf(stderr, "Cannot connect to Pisces XPMEM channel\n");
	return -1;
    }

    if (!(list_size % BUF_SIZE)) {
	struct xpmem_dom * tmp = realloc(xpmem_domain_list, sizeof(struct xpmem_dom) * (list_size + BUF_SIZE));
	if (!tmp) {
	    perror("realloc");
	    return -1;
	}

	xpmem_domain_list = tmp;
    }

    /* Set pisces_dom pointer */
    pisces_dom = &(xpmem_domain_list[list_size]);

    xpmem_domain_list[list_size].enclave.id = 0;
    xpmem_domain_list[list_size].enclave.fd = xpmem_fd;
    xpmem_domain_list[list_size++].enclave.type = ENCLAVE;

    return 0;
}

int xpmem_pisces_add_local_dom(void) {
    int fd = 0, xpmem_fd = 0;

    fd = open("/xpmem", O_RDWR);
    if (fd == -1) {
	fprintf(stderr, "Cannot open /xpmem: cannot connect to local XPMEM channel\n");
	return -1;
    }

    if (ioctl(fd, XPMEM_CMD_EXT_LOCAL_CONNECT, NULL) == -1) {
        fprintf(stderr, "Cannot connect to local XPMEM channel\n");
        return -1;
    }

    if (ioctl(fd, XPMEM_CMD_EXT_REMOTE_CONNECT, NULL) == -1) {
        fprintf(stderr, "Cannot connect to remote XPMEM channel\n");
        return -1;
    }

    close(fd);

    {
	xpmem_fd = open("/xpmem-local", O_RDWR);
	if (xpmem_fd <= 0) {
	    fprintf(stderr, "Cannot open /xpmem-local: cannot connect to local XPMEM channel\n");
	    return -1;
	}

	if (!(list_size % BUF_SIZE)) {
	    struct xpmem_dom * tmp = realloc(xpmem_domain_list, sizeof(struct xpmem_dom) * (list_size + BUF_SIZE));
	    if (!tmp) {
		perror("realloc");
		return -1;
	    }

	    xpmem_domain_list = tmp;
	}

	xpmem_domain_list[list_size].enclave.id = xpmem_fd;
	xpmem_domain_list[list_size].enclave.fd = xpmem_fd;
	xpmem_domain_list[list_size++].enclave.type = LOCAL;
    }

    {
	xpmem_fd = open("/xpmem-remote", O_RDWR);
	if (xpmem_fd <= 0) {
	    fprintf(stderr, "Cannot open /xpmem-remote: cannot connect to remote XPMEM channel\n");
	    return -1;
	}

	if (!(list_size % BUF_SIZE)) {
	    struct xpmem_dom * tmp = realloc(xpmem_domain_list, sizeof(struct xpmem_dom) * (list_size + BUF_SIZE));
	    if (!tmp) {
		perror("realloc");
		return -1;
	    }

	    xpmem_domain_list = tmp;
	}

	xpmem_domain_list[list_size].enclave.id = xpmem_fd;
	xpmem_domain_list[list_size].enclave.fd = xpmem_fd;
	xpmem_domain_list[list_size++].enclave.type = LOCAL;
    }

    return 0;
}

int xpmem_pisces_add_dom(int fd, int dom_id) {
    int xpmem_fd = 0;
    char vm_str[16];

    if (ioctl(fd, V3_VM_XPMEM_CONNECT, NULL) == -1) {
	fprintf(stderr, "Cannot connect to VM %d\n", dom_id);
	return -1;
    }

    snprintf(vm_str, 16, "/v3-vm%d-xpmem", dom_id);

    xpmem_fd = open(vm_str, O_RDWR);
    if (xpmem_fd <= 0) {
	fprintf(stderr, "Cannot connect to VM %d XPMEM channel\n", dom_id);
	return -1;
    }

    if (!(list_size % BUF_SIZE)) {
	struct xpmem_dom * tmp = realloc(xpmem_domain_list, sizeof(struct xpmem_dom) * (list_size + BUF_SIZE));
	if (!tmp) {
	    perror("realloc");
	    return -1;
	}

	xpmem_domain_list = tmp;
    }

    printf("Connected to VM %d XPMEM channel\n", dom_id);

    xpmem_domain_list[list_size].enclave.id = dom_id;
    xpmem_domain_list[list_size].enclave.fd = xpmem_fd;
    xpmem_domain_list[list_size++].enclave.type = VM;
    return 0;
}

struct xpmem_dom * xpmem_pisces_get_dom_list(int * size) {
    *size = list_size;
    return xpmem_domain_list;
}

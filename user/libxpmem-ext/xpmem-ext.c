#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <xpmem-ext.h>
#include <lwk/liblwk.h>

static int xpmem_fd = -1;


int xpmem_init(void)
{
    xpmem_fd = open(XPMEM_DEV_PATH, O_RDWR);
    if (xpmem_fd == -1) {
	return -1;
    }

    return 0;
}

int xpmem_ioctl(int cmd, void * arg)
{
    int ret;
    if (xpmem_fd == -1 && xpmem_init() != 0) {
	return -1;
    }

    ret = ioctl(xpmem_fd, cmd, arg);
    return ret;
}


xpmem_segid_t xpmem_make(void *vaddr, size_t size, int permit_type, void *permit_value)
{
    struct xpmem_cmd_make make_info;

    make_info.vaddr = (__u64)vaddr;
    make_info.size = size;
    make_info.permit_type = permit_type;
    make_info.permit_value = (__u64)permit_value;
    if (xpmem_ioctl(XPMEM_CMD_MAKE, &make_info) == -1 || !make_info.segid)
	return -1;

    return make_info.segid;
}


int xpmem_remove(xpmem_segid_t segid)
{
    struct xpmem_cmd_remove remove_info;

    remove_info.segid = segid;
    if (xpmem_ioctl(XPMEM_CMD_REMOVE, &remove_info) == -1)
	return -1;
    return 0;
}


xpmem_apid_t xpmem_get(xpmem_segid_t segid, int flags, int permit_type, void *permit_value)
{
    struct xpmem_cmd_get get_info;

    get_info.segid = segid;
    get_info.flags = flags;
    get_info.permit_type = permit_type;
    get_info.permit_value = (__u64)NULL;
    if (xpmem_ioctl(XPMEM_CMD_GET, &get_info) == -1 || !get_info.apid)
	return -1;
    return get_info.apid;
}


int xpmem_release(xpmem_apid_t apid)
{
    struct xpmem_cmd_release release_info;

    release_info.apid = apid;
    if (xpmem_ioctl(XPMEM_CMD_RELEASE, &release_info) == -1)
	return -1;
    return 0;
}


void *xpmem_attach(struct xpmem_addr addr, size_t size, void *vaddr)
{
    struct xpmem_cmd_attach attach_info;

    attach_info.apid = addr.apid;
    attach_info.offset = addr.offset;
    attach_info.size = size;
    attach_info.vaddr = (__u64)vaddr;
    attach_info.fd = xpmem_fd;
    attach_info.flags = 0;
    if (xpmem_ioctl(XPMEM_CMD_ATTACH, &attach_info) == -1)
	return (void *)-1;
    return (void *)attach_info.vaddr;
}


int xpmem_detach(void *vaddr)
{
    struct xpmem_cmd_detach detach_info;

    detach_info.vaddr = (__u64)vaddr;
    if (xpmem_ioctl(XPMEM_CMD_DETACH, &detach_info) == -1)
	return -1;
    return 0;
}

int xpmem_version(void)
{
    return xpmem_ioctl(XPMEM_CMD_VERSION, NULL);
}

/* 
 * LCALL based file I/O
 * (c) Jack Lange, 2013
 */

#include <lwk/types.h>
#include <lwk/string.h>
#include <arch/page.h>
#include <lwk/kernel.h>

#include <lwk/print.h>

#include <arch/pisces/pisces_lcall.h>
#include <arch/pisces/pisces_file.h>


u64 pisces_file_open(const char * path, int mode) {
    struct vfs_open_lcall * open_lcall = NULL;
    //struct pisces_lcall_resp * resp = NULL;
    struct pisces_lcall_resp * open_resp = NULL;
    u64 ret = 0;
    u32 lcall_len = sizeof(struct vfs_open_lcall) + strlen(path) + 1;

    open_lcall = kmem_alloc(lcall_len);

    memset(open_lcall, 0, lcall_len);

    open_lcall->lcall.lcall = PISCES_LCALL_VFS_OPEN;
    open_lcall->lcall.data_len = lcall_len - sizeof(struct pisces_lcall);

    open_lcall->mode = (u32)mode;
    strncpy(open_lcall->path, path, strlen(path));

    pisces_lcall_exec((struct pisces_lcall *)open_lcall, (struct pisces_lcall_resp **)&open_resp);

    ret = open_resp->status;

    kmem_free(open_lcall);
    kmem_free(open_resp);

    return ret;
}


int pisces_file_close(u64 file_handle) {
    struct vfs_close_lcall close_lcall;
    //struct pisces_lcall_resp * resp = NULL;
    struct pisces_lcall_resp * close_resp = NULL;
    u64 ret = 0;

    close_lcall.lcall.lcall = PISCES_LCALL_VFS_CLOSE;
    close_lcall.lcall.data_len = sizeof(struct vfs_close_lcall) - sizeof(struct pisces_lcall);

    close_lcall.file_handle = file_handle;

    pisces_lcall_exec((struct pisces_lcall *)&close_lcall, (struct pisces_lcall_resp **)&close_resp);

    ret = close_resp->status;
    kmem_free(close_resp);

    return ret;
}

u64 pisces_file_size(u64 file_handle) {
    struct vfs_size_lcall size_lcall;
    //struct pisces_lcall_resp * resp = NULL;
    struct pisces_lcall_resp * size_resp = NULL;
    u64 ret = 0;

    size_lcall.lcall.lcall = PISCES_LCALL_VFS_SIZE;
    size_lcall.lcall.data_len = sizeof(struct vfs_size_lcall) - sizeof(struct pisces_lcall);

    size_lcall.file_handle = file_handle;

    pisces_lcall_exec((struct pisces_lcall *)&size_lcall, (struct pisces_lcall_resp **)&size_resp);

    ret = size_resp->status;

    kmem_free(size_resp);
    return ret;
}



u64 pisces_file_read(u64 file_handle, void * buffer, 
		     u64 length, u64 offset) {
    struct vfs_read_lcall * read_lcall = NULL;
    //struct pisces_lcall_resp * resp = NULL;
    struct pisces_lcall_resp * read_resp = NULL;
    u32 lcall_len = sizeof(struct vfs_read_lcall) + sizeof(struct vfs_buf_desc);
    u64 ret = 0;

    read_lcall = kmem_alloc(lcall_len);

    if (read_lcall == 0) {
	while (1);
	return (u64)-1;
    }

    memset(read_lcall, 0, lcall_len);

    read_lcall->lcall.lcall = PISCES_LCALL_VFS_READ;
    read_lcall->lcall.data_len = lcall_len - sizeof(struct pisces_lcall);

    read_lcall->file_handle = file_handle;
    read_lcall->offset = offset;
    read_lcall->length = length;
    read_lcall->num_descs = 1;
    read_lcall->descs[0].size = length;
    read_lcall->descs[0].phys_addr = __pa(buffer);

    pisces_lcall_exec((struct pisces_lcall *)read_lcall, (struct pisces_lcall_resp **)&read_resp);


    ret = read_resp->status;

    kmem_free(read_lcall);
    kmem_free(read_resp);

    return ret;
}


u64 pisces_file_write(u64 file_handle, void * buffer, 
		     u64 length, u64 offset) {
    struct vfs_write_lcall  * write_lcall = NULL;
    //struct pisces_lcall_resp * resp = NULL;
    struct pisces_lcall_resp * write_resp = NULL;
    u32 lcall_len = sizeof(struct vfs_write_lcall) + sizeof(struct vfs_buf_desc);
    u64 ret = 0;

    write_lcall = kmem_alloc(lcall_len);
 
    memset(write_lcall, 0, lcall_len);

    write_lcall->lcall.lcall = PISCES_LCALL_VFS_WRITE;
    write_lcall->lcall.data_len = sizeof(struct vfs_write_lcall) - sizeof(struct pisces_lcall);
    write_lcall->lcall.data_len += sizeof(struct vfs_buf_desc);

    write_lcall->file_handle = file_handle;
    write_lcall->offset = offset;
    write_lcall->length = length;
    write_lcall->num_descs = 1;
    write_lcall->descs[0].size = length;
    write_lcall->descs[0].phys_addr = __pa(buffer);

    pisces_lcall_exec((struct pisces_lcall *)write_lcall, (struct pisces_lcall_resp **)&write_resp);
    
    ret = write_resp->status;

    kmem_free(write_lcall);
    kmem_free(write_resp);

    return ret;
}

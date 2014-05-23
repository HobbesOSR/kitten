/* 
 * LCALL based file I/O
 * (c) Jack Lange, 2013
 */

#include <arch-generic/fcntl.h>
#include <lwk/types.h>

#include <arch/pisces/pisces_lcall.h>




/* VFS LCALLs */
struct vfs_buf_desc {
	u32 size;
	u64 phys_addr;
} __attribute__((packed));


struct vfs_read_lcall {
	struct pisces_lcall lcall;

	u64 file_handle;
	u64 offset;
	u64 length;

	u32 num_descs;
	struct vfs_buf_desc descs[0];
} __attribute__((packed));

struct vfs_write_lcall {
	struct pisces_lcall lcall;

	u64 file_handle;
	u64 offset;
	u64 length;

	u32 num_descs;
	struct vfs_buf_desc descs[0];
} __attribute__((packed));


struct vfs_open_lcall {
	struct pisces_lcall lcall;

	u32 mode;
	u8  path[0];
} __attribute__((packed));

struct vfs_close_lcall {
	struct pisces_lcall lcall;

	u64 file_handle;
} __attribute__((packed));

struct vfs_size_lcall {
	struct pisces_lcall lcall;

	u64 file_handle;
} __attribute__((packed));




/* 
 *  MODE: (O_RDWR, O_RDONLY, O_WRONLY, O_CREAT)
 */

u64 pisces_file_open(const char * path, int mode);
int pisces_file_close(u64 file_handle);

loff_t pisces_file_size(u64 file_handle);

ssize_t 
pisces_file_read(u64      file_handle, 
		 void   * buffer, 
		 size_t   length, 
		 loff_t   offset);
ssize_t
pisces_file_write(u64      file_handle, 
		  void   * buffer, 
		  size_t   length, 
		  loff_t   offset);



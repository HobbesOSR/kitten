/** \file
 * Kernel virtual filesystem
 */
#ifndef _lwk_kfs_h_
#define _lwk_kfs_h_

#include <lwk/list.h>
#include <lwk/task.h>


struct kfs_fops
{
	struct kfs_file * (*lookup)(
		struct kfs_file *	root,
		const char *		name,
		unsigned		create_mode
	);

	int (*open)(
		struct kfs_file *
	);

	int (*close)(
		struct kfs_file *
	);

	ssize_t (*read)(
		struct kfs_file *,
		uaddr_t,
		size_t
	);

	ssize_t (*write)(
		struct kfs_file *,
		uaddr_t,
		size_t
	);
};

#define MAX_PATHLEN		1024

struct kfs_file
{
	struct htable *		files;
	struct hlist_node	ht_link;
	lwk_id_t		name_ptr;

	struct kfs_file *	parent;
	char			name[ 128 ];
	const struct kfs_fops *	fops;
	unsigned		mode;
	void *			priv;
	size_t			priv_len;
	unsigned		refs;
};


extern struct kfs_file *
kfs_mkdirent(
	struct kfs_file *	parent,
	const char *		name,
	const struct kfs_fops *	fops,
	unsigned		mode,
	void *			priv,
	size_t			priv_len
);


void
kfs_destroy(
	struct kfs_file *	file
);


static inline struct kfs_file *
get_current_file(
	int			fd
)
{
	if( fd < 0 || fd > MAX_FILES )
		return 0;
	return current->files[ fd ];
}


extern struct kfs_file * kfs_root;


extern struct kfs_file *
kfs_lookup(
	struct kfs_file *	root,
	const char *		dirname,
	unsigned		create_mode
);

#endif

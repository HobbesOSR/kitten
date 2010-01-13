struct vfsmount {
	struct dentry *mnt_root;        /* root of the mounted tree */
	const char *mnt_devname;
};

static inline void mntput(struct vfsmount *mnt)
{
        if (mnt) {
                /* kill mount point ?*/
        }
}


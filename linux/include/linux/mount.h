struct vfsmount {
	struct dentry *mnt_root;        /* root of the mounted tree */
	const char *mnt_devname;
	struct super_block *mnt_sb;
};

static inline void mntput(struct vfsmount *mnt)
{
        if (mnt) {
                /* kill mount point ?*/
        }
}


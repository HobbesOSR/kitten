#ifndef _LINUX_NAMEI_H
#define _LINUX_NAMEI_H

enum { MAX_NESTED_LINKS = 8 };

struct nameidata {
	unsigned int    flags;
	int             last_type;
	unsigned        depth;
	char *saved_names[MAX_NESTED_LINKS + 1];
};

extern struct dentry *lookup_one_len(const char *, struct dentry *, int);

#endif /* _LINUX_NAMEI_H */

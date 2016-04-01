/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LWK_SYSFS_H_
#define	_LWK_SYSFS_H_

#include <lwk/kfs.h>

struct kobject;

#define sysfs_dirent inode

struct attribute {
	const char 	*name;
	struct module	*owner;
	mode_t		mode;
};

struct sysfs_ops {
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *, const char *,
	    size_t);
};

struct attribute_group {
	const char		*name;
	mode_t                  (*is_visible)(struct kobject *,
				    struct attribute *, int);
	struct attribute	**attrs;
};

#define	__ATTR(_name, _mode, _show, _store) {				\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
        .show = _show, .store  = _store,				\
}

#define	__ATTR_RO(_name) {						\
	.attr = { .name = __stringify(_name), .mode = 0444 },		\
	.show   = _name##_show,						\
}

#define	__ATTR_NULL	{ .attr = { .name = NULL } }



extern 	int
sysfs_create_dir(struct kobject * kobj);


extern int
sysfs_create_file(struct kobject *kobj, const struct attribute *attr);

extern void
sysfs_remove_file(struct kobject *kobj, const struct attribute *attr);

extern void
sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp);

extern void
sysfs_remove_dir(struct kobject *kobj);

extern int
sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);





#define sysfs_attr_init(attr) do {} while(0)

#endif	/* _LINUX_SYSFS_H_ */

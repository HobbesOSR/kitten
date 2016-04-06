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
#ifndef	_KOBJECT_H_
#define	_KOBJECT_H_

#include <lwk/kref.h>
#include <lwk/sysfs.h>

struct kobject;

struct kobj_type {
	void (*release)(struct kobject *kobj);
	const struct sysfs_ops * sysfs_ops;
	struct attribute ** default_attrs;
};

extern struct kobj_type kfree_type;

struct kobject {
	struct kobject	     * parent;
	char		     * name;
	struct kref	       kref;
	struct kobj_type     * ktype;
	struct list_head       entry;
	struct sysfs_dirent  * sd;
};



/* Kitten Doesn't support Hotplug, so these become No-ops */

enum kobject_action {
    KOBJ_ADD,
    KOBJ_REMOVE,
    KOBJ_CHANGE,
    KOBJ_MOVE,
    KOBJ_ONLINE,
    KOBJ_OFFLINE,
    KOBJ_MAX
};

struct kobj_uevent_env {};

static inline int kobject_uevent(struct kobject      * kobj,
				 enum kobject_action   action)
{ return 0; }

static inline int add_uevent_var(struct kobj_uevent_env * env,
				 const char             * format, 
				 ...)
{ return 0; }


/** ** **/


void
kobject_init(struct kobject   * kobj, 
	     struct kobj_type * ktype);



struct kobject * kobject_create(void);
void             kobject_release(struct kref *kref);


void             kobject_put(struct kobject * kobj);
struct kobject * kobject_get(struct kobject * kobj);


int
kobject_set_name_vargs(struct kobject * kobj,
		       const char     * fmt, 
		       va_list          args);

int
kobject_add(struct kobject * kobj,
	    struct kobject * parent,
	    const char     * fmt, 
	    ...);


struct kobject *
kobject_create_and_add(const char     * name,
		       struct kobject * parent);


int
kobject_init_and_add(struct kobject   * kobj, 
		     struct kobj_type * ktype,
		     struct kobject   * parent, 
		     const char       * fmt,
		     ...);

int 
kobject_set_name(struct kobject * kobj,
		 const char     * fmt,
		 ...);


static inline char *
kobject_name(const struct kobject *kobj)
{

	return kobj->name;
}



#endif /* _KOBJECT_H_ */

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

#include <lwk/kobject.h>


static void
kobject_kfree(struct kobject *kobj)
{

	kmem_free(kobj);
}

struct kobj_type kfree_type = { .release = kobject_kfree };



void
kobject_init(struct kobject   * kobj, 
	     struct kobj_type * ktype)
{
	kref_init(&kobj->kref);
	INIT_LIST_HEAD(&kobj->entry);
	kobj->ktype = ktype;
	kobj->sd    = NULL;
}



int
kobject_set_name_vargs(struct kobject * kobj,
		       const char     * fmt, 
		       va_list          args)
{
	char * old  = NULL;
	char * name = NULL;

	old = kobj->name;

	if ( (old != NULL) && 
	     (fmt == NULL) ) 
	{
		return 0;
	}

	name = kvasprintf(0, fmt, args);

	if (!name) {
		return -ENOMEM;
	}

	kobj->name = name;
	kmem_free(old);

	for (; *name != '\0'; name++) {
		if (*name == '/') {
			*name = '!';
		}
	}

	return (0);
}

int
kobject_set_name(struct kobject * kobj, 
		 const char     * fmt, 
		 ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);

	return (error);
}

static inline int
kobject_add_complete(struct kobject * kobj,
		     struct kobject * parent)
{
	struct kobj_type * t = NULL;
	int error;

	kobj->parent = kobject_get(parent);

	error        = sysfs_create_dir(kobj);

	if ( (error                      == 0) && 
	     (kobj->ktype                != 0) && 
	     (kobj->ktype->default_attrs != 0) ) 
	{
		struct attribute ** attr;

		t = kobj->ktype;

		for (attr = t->default_attrs; *attr != NULL; attr++) {
			error = sysfs_create_file(kobj, *attr);

			if (error) {
				break;
			}
		}

		if (error) {
			sysfs_remove_dir(kobj);
		}
	}

	return (error);
}



int
kobject_init_and_add(struct kobject   * kobj, 
		     struct kobj_type * ktype,
		     struct kobject   * parent, 
		     const char        *fmt,
		     ...)
{
	va_list args;
	int error;

	kobject_init(kobj, ktype);
	kobj->ktype    = ktype;
	kobj->parent   = parent;
	kobj->name     = NULL;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);

	if (error) {
		return (error);
	}

	return kobject_add_complete(kobj, parent);
}


struct kobject *
kobject_create(void)
{
	struct kobject * kobj = NULL;

	kobj = kmem_alloc(sizeof(*kobj));

	if (kobj == NULL) {
		return (NULL);
	}

	kobject_init(kobj, &kfree_type);

	return (kobj);
}


struct kobject *
kobject_create_and_add(const char     * name, 
		       struct kobject * parent)
{
	struct kobject *kobj;

	kobj = kobject_create();

	if (kobj == NULL) {
		return (NULL);
	}

	if (kobject_add(kobj, parent, "%s", name) == 0) {
		return (kobj);
	}

	kobject_put(kobj);

	return (NULL);
}



int
kobject_add(struct kobject * kobj, 
	    struct kobject * parent, 
	    const char     * fmt,
	    ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);

	if (error) {
		return (error);
	}

	return kobject_add_complete(kobj, parent);
}

void
kobject_release(struct kref * kref)
{
	struct kobject * kobj = NULL;
	char           * name = NULL;

	kobj = container_of(kref, struct kobject, kref);

	sysfs_remove_dir(kobj);

	if (kobj->parent) {
		kobject_put(kobj->parent);
	}

	kobj->parent = NULL;
	name         = kobj->name;

	if ( (kobj->ktype) && 
	     (kobj->ktype->release) )
	{
		kobj->ktype->release(kobj);
	}

	kmem_free(name);
}




void
kobject_put(struct kobject * kobj)
{
	if (kobj) {
		kref_put(&kobj->kref, kobject_release);
	}
}

struct kobject *
kobject_get(struct kobject *kobj)
{

	if (kobj) {
		kref_get(&kobj->kref);
	}

	return kobj;
}

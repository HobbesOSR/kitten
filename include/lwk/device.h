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
#ifndef	_LWK_DEVICE_H_
#define	_LWK_DEVICE_H_



#include <lwk/kernel.h>
#include <lwk/kobject.h>
#include <lwk/sysfs.h>

#include <asm/atomic.h>


#include <lwk/interrupt.h>

struct device;

struct class {
	const char	* name;
	struct module	* owner;
	struct kobject	  kobj;

	struct class_attribute	* class_attrs;
	struct device_attribute	* dev_attrs;

	void (*class_release)(struct class  * class);
	void (*dev_release)  (struct device * dev);

	//	struct class_private *p;
};




struct device {
	struct device  * parent;


	dev_t		 devt ;
	struct class   * class;

	void		 (*release)(struct device *dev);
	struct kobject 	 kobj;


	uint64_t         dma_mask;
	void	       * driver_data;
	unsigned int 	 irq;
	//	unsigned int	msix;
	//	unsigned int	msix_max;
};


struct class_attribute {
	struct attribute	attr;
        ssize_t			(*show) (struct class *, char *);
        ssize_t			(*store)(struct class *, const char *, size_t);
};
#define	CLASS_ATTR(_name, _mode, _show, _store)				\
	struct class_attribute class_attr_##_name =			\
	    { { #_name, NULL, _mode }, _show, _store }

struct device_attribute {
	struct attribute attr;

	ssize_t (*show)(struct device           *,
			struct device_attribute *,
			char                    *);

	ssize_t (*store)(struct device           *,
			 struct device_attribute *,
			 const char              *,
			 size_t);
};

#define	DEVICE_ATTR(_name, _mode, _show, _store)			\
	struct device_attribute dev_attr_##_name =			\
	    { { #_name, NULL, _mode }, _show, _store }



/* debugging and troubleshooting/diagnostic helpers. */

static inline 
const char * 
dev_driver_string(const struct device * dev)
{
	/*
	return dev->driver ? dev->driver->name :
			(dev->bus ? dev->bus->name :
			(dev->class ? dev->class->name : ""));
	*/
	return dev->class ? dev->class->name : "";
}

#define dev_printk(level, dev, format, arg...)	\
	printk(level "%s %s: " format , dev_driver_string(dev) , \
	       dev_name(dev) , ## arg)

#define dev_emerg(dev, format, arg...)		\
	dev_printk(KERN_EMERG , dev , format , ## arg)
#define dev_alert(dev, format, arg...)		\
	dev_printk(KERN_ALERT , dev , format , ## arg)
#define dev_crit(dev, format, arg...)		\
	dev_printk(KERN_CRIT  , dev , format , ## arg)
#define dev_err(dev, format, arg...)		\
	dev_printk(KERN_ERR , dev , format , ## arg)
#define dev_warn(dev, format, arg...)		\
	dev_printk(KERN_WARNING , dev , format , ## arg)
#define dev_notice(dev, format, arg...)		\
	dev_printk(KERN_NOTICE ,  dev , format , ## arg)
#define dev_info(dev, format, arg...)		\
	dev_printk(KERN_INFO , dev , format , ## arg)



static inline void *
dev_get_drvdata(struct device * dev)
{
	return dev->driver_data;
}

static inline void
dev_set_drvdata(struct device * dev, 
		void          * data)
{
	dev->driver_data = data;
}

struct device * get_device(struct device * dev);
void            put_device(struct device * dev);


char * dev_name(const struct device * dev);

#define	dev_set_name(_dev, _fmt, ...)					\
	kobject_set_name(&(_dev)->kobj, (_fmt), ##__VA_ARGS__)



void device_init(struct device * dev, 
		 struct class  * class, 
		 struct device * parent,
		 dev_t           devt, 
		 void          * drvdata,
		 const char    * fmt,
		 ...);

struct device * device_create(struct class  * class, 
			      struct device * parent,
			      dev_t           devt, 
			      void          * drvdata,
			      const char    * fmt,
			      ...);

void
device_destroy(struct class * class,
	       dev_t          devt);


int  device_register  (struct device * dev);
void device_unregister(struct device * dev);





struct class * class_create(struct module * owner, 
			    const char    * name);

void class_destroy(struct class * class);

int  class_register  (struct class * class);
void class_unregister(struct class * class);


int
device_create_file(struct device                 * dev,
		   const struct device_attribute * attr);

void
device_remove_file(struct device                 * dev, 
		   const struct device_attribute * attr);

int
class_create_file(struct class                 * class, 
		  const struct class_attribute * attr);

void
class_remove_file(struct class                 * class, 
		  const struct class_attribute * attr);


/* Only needed by Linux drivers. TODO Move to compatibility layer */
static inline int 
dev_to_node(struct device * dev)
{
                return -1;
}

int init_devices(void);


#endif	/* _LWK_DEVICE_H_ */

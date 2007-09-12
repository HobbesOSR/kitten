#ifndef _LWK_PARAMS_H
#define _LWK_PARAMS_H
/* (C) Copyright 2001, 2002 Rusty Russell IBM Corporation */
#include <lwk/init.h>
#include <lwk/stringify.h>
#include <lwk/kernel.h>

struct kernel_param;

/* Returns 0, or -errno.  arg is in kp->arg. */
typedef int (*param_set_fn)(const char *val, struct kernel_param *kp);
/* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
typedef int (*param_get_fn)(char *buffer, struct kernel_param *kp);

struct kernel_param {
	const char *	name;
	param_set_fn	set;
	param_get_fn	get;
	void *		arg;
};

/* Special one for strings we want to copy into */
struct kparam_string {
	unsigned int	maxlen;
	char *		string;
};

/* Special one for arrays */
struct kparam_array {
	unsigned int	max;
	unsigned int *	num;
	param_set_fn	set;
	param_get_fn	get;
	unsigned int	elemsize;
	void *		elem;
};

/* This is the fundamental function for registering kernel parameters. */
#define __param_call(prefix, name, set, get, arg)			\
	static char __param_str_##name[] = prefix #name;		\
	static struct kernel_param const __param_##name			\
	__attribute_used__						\
    __attribute__ ((unused,__section__ ("__param"),aligned(sizeof(void *)))) \
	= { __param_str_##name, set, get, arg }

/* Helper functions: type is byte, short, ushort, int, uint, long,
   ulong, charp, bool or invbool, or XXX if you define param_get_XXX,
   param_set_XXX and param_check_XXX. */
#define __PARAM_NAMED(prefix, name, value, type, description) 		\
	param_check_##type(name, &(value));				\
	__param_call(prefix, name, param_set_##type, param_get_##type, &value)

#define __PARAM(prefix, name, type, description)			\
	__PARAM_NAMED(prefix, name, name, type, description)

/* Actually copy string: maxlen param is usually sizeof(string). */
#define __PARAM_STRING(prefix, name, string, len, description)		\
	static struct kparam_string __param_string_##name		\
		= { len, string };					\
	__param_call(prefix, name, param_set_copystring,		\
		   param_get_string, &__param_string_##name)

/* Comma-separated array: *nump is set to number they actually specified. */
#define __PARAM_ARRAY_NAMED(prefix, name, array, type, nump, description) \
	static struct kparam_array __param_arr_##name			  \
	= { ARRAY_SIZE(array), nump, param_set_##type, param_get_##type,  \
	    sizeof(array[0]), array };					  \
	__param_call(prefix, name, param_array_set, param_array_get, 	  \
			  &__param_arr_##name)

#define __PARAM_ARRAY(prefix, name, type, nump, description)		  \
	__PARAM_ARRAY_NAMED(prefix, name, name, type, nump, description)


/*
 * Driver parameters will have the name of the driver followed
 * by a '.' pre-prepended to the parameter name.  For example,
 * if the serial port driver defines the parameter:
 *
 *     driver_param(debug, int, "debug level");
 *
 * Then the user could set debug on the kernel command line by
 * specifying:
 *
 *     serial.debug=1
 *
 * This assumes that the driver_param() was defined in serial.c.
 * KBUILD_MODNAME is set to the name of the file being compiled.
 * Alternatively, you could set it before including lwk/params.h.
 */
#define DRIVER_PARAM_PREFIX	KBUILD_MODNAME "."

#define DRIVER_PARAM(name, type, description) \
	__PARAM(DRIVER_PARAM_PREFIX, name, type, description)

#define DRIVER_PARAM_NAMED(name, value, type, description) \
	__PARAM_NAMED(DRIVER_PARAM_PREFIX, name, value, type, description)

#define DRIVER_PARAM_STRING(name, string, len, description) \
	__PARAM_STRING(DRIVER_PARAM_PREFIX, name, string, len, description)


/*
 * Core kernel parameters are just raw... they have no prefix.
 */
#define KERNEL_PARAM(name, type, description) \
	__PARAM("", name, type, description)

#define KERNEL_PARAM_NAMED(name, value, type, description) \
	__PARAM_NAMED("", name, value, type, description)

#define KERNEL_PARAM_STRING(name, string, len, description) \
	__PARAM_STRING("", name, string, len, description)


/* Called at kernel boot */
extern int parse_args(const char *name,
		      char *args,
		      struct kernel_param *params,
		      unsigned num,
		      int (*unknown)(char *param, char *val));

/* All the helper functions */
/* The macros to do compile-time type checking stolen from Jakub
   Jelinek, who IIRC came up with this idea for the 2.4 module init code. */
#define __param_check(name, p, type) \
	static inline type *__check_##name(void) { return(p); }

extern int param_set_byte(const char *val, struct kernel_param *kp);
extern int param_get_byte(char *buffer, struct kernel_param *kp);
#define param_check_byte(name, p) __param_check(name, p, unsigned char)

extern int param_set_short(const char *val, struct kernel_param *kp);
extern int param_get_short(char *buffer, struct kernel_param *kp);
#define param_check_short(name, p) __param_check(name, p, short)

extern int param_set_ushort(const char *val, struct kernel_param *kp);
extern int param_get_ushort(char *buffer, struct kernel_param *kp);
#define param_check_ushort(name, p) __param_check(name, p, unsigned short)

extern int param_set_int(const char *val, struct kernel_param *kp);
extern int param_get_int(char *buffer, struct kernel_param *kp);
#define param_check_int(name, p) __param_check(name, p, int)

extern int param_set_uint(const char *val, struct kernel_param *kp);
extern int param_get_uint(char *buffer, struct kernel_param *kp);
#define param_check_uint(name, p) __param_check(name, p, unsigned int)

extern int param_set_long(const char *val, struct kernel_param *kp);
extern int param_get_long(char *buffer, struct kernel_param *kp);
#define param_check_long(name, p) __param_check(name, p, long)

extern int param_set_ulong(const char *val, struct kernel_param *kp);
extern int param_get_ulong(char *buffer, struct kernel_param *kp);
#define param_check_ulong(name, p) __param_check(name, p, unsigned long)

extern int param_set_charp(const char *val, struct kernel_param *kp);
extern int param_get_charp(char *buffer, struct kernel_param *kp);
#define param_check_charp(name, p) __param_check(name, p, char *)

extern int param_set_bool(const char *val, struct kernel_param *kp);
extern int param_get_bool(char *buffer, struct kernel_param *kp);
#define param_check_bool(name, p) __param_check(name, p, int)

extern int param_set_invbool(const char *val, struct kernel_param *kp);
extern int param_get_invbool(char *buffer, struct kernel_param *kp);
#define param_check_invbool(name, p) __param_check(name, p, int)

extern int param_array_set(const char *val, struct kernel_param *kp);
extern int param_array_get(char *buffer, struct kernel_param *kp);

extern int param_set_copystring(const char *val, struct kernel_param *kp);
extern int param_get_string(char *buffer, struct kernel_param *kp);

extern int parse_params(const char *str);
extern int param_set_by_name_int(char *param, int val);

/*
 * These two symbols are defined by the platform's linker script.
 * They surround a table of kernel parameter descriptors.  This table
 * is used by the command line parser to determine how each argument
 * should be handled... each encountered argument causes a search of
 * this table.
 */
extern struct kernel_param __start___param[], __stop___param[];

#endif /* _LWK_PARAMS_H */

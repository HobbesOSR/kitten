#ifndef _LWK_CONSOLE_H
#define _LWK_CONSOLE_H

#include <lwk/list.h>
#include <lwk/compiler.h>

/** Console structure.
 *
 * Each console in the system is represented by one of these
 * structures.  A console driver (e.g., VGA, Serial) fills in
 * one of these structures and passes it to ::console_register().
 */
struct console {
	char	name[64];
	void	(*write)(struct console *, const char *);
	void *	private_data;

	struct list_head next;
};

/** Console driver structure.
 *
 * Each console driver must register one of these structures in the
 * console_driver_table via the REGISTER_CONSOLE_DRIVER() macro.
 */
struct console_driver {
	const char *	name;	
	void 		(*init)(void);
};
aligncheck(struct console_driver, 16);

#define REGISTER_CONSOLE_DRIVER(name,init)				\
	static char __driver_name[] = #name;				\
	static struct console_driver const __console_driver		\
	  __attribute_used__						\
	__attribute__ ((unused,__section__ ("__console_driver_table"),	\
		       aligned(sizeof(void *))))			\
	= { __driver_name, init }

extern void console_register(struct console *);
extern void console_write(const char *);
extern void init_console(void);

/* These two symbols surround the console driver table. */
extern struct console_driver __start___console_driver_table[],
                             __stop___console_driver_table[];

#endif

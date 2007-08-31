#ifndef _LWK_CONSOLE_H
#define _LWK_CONSOLE_H

#include <lwk/list.h>

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

extern void console_register(struct console *);
extern void console_write(const char *);

#endif

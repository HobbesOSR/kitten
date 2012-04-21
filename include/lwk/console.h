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
	void    (*poll_put_char)(struct console *, unsigned char);
	char    (*poll_get_char)(struct console *);
	void *	private_data;

	struct list_head next;
};

extern void console_register(struct console *);
extern void console_write(const char *);
extern void console_inbuf_add(char c);
extern ssize_t console_inbuf_read(char *buf, size_t len);
extern void console_init(void);

#endif

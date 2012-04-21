/*
 * Console input buffer functionality added by:
 * Jedidiah R. McClurg
 * Northwestern University
 * 4-18-2012
 */

#include <lwk/kernel.h>
#include <lwk/console.h>
#include <lwk/spinlock.h>
#include <lwk/waitq.h>
#include <lwk/sched.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/errno.h>
#include <arch/uaccess.h>

/**
 * List of all registered consoles in the system.
 *
 * Kernel messages output via printk() will be written to
 * all consoles in this list.
 */
static LIST_HEAD(console_list);

/**
 * Serializes access to the console.
 */
static DEFINE_SPINLOCK(console_lock);

/**
 * Holds a comma separated list of consoles to configure.
 */
static char console_str[128];
param_string(console, console_str, sizeof(console_str));

/**
 * Console input buffer.
 *
 * The console input buffer is implemented as a FIFO.
 * It would probably be better to use a circular buffer,
 * but this is good enough for now.
 */
#define CONSOLE_INBUF_SIZE 512
static char console_inbuf[CONSOLE_INBUF_SIZE];
static size_t console_inbuf_len;
static bool console_inbuf_has_newline = false;
static DEFINE_SPINLOCK(console_inbuf_lock);

/**
 * Console input buffer wait queue.
 *
 * If no console input is available, a task reading from
 * the console input buffer will go to sleep on this wait queue.
 */
static DECLARE_WAITQ(console_inbuf_waitq);

/**
 * Registers a new console.
 */
void console_register(struct console *con)
{
	list_add(&con->next, &console_list);
}

/**
 * Writes a string to all registered consoles.
 */
void console_write(const char *str)
{
	struct console *con;
	unsigned long flags;

	spin_lock_irqsave(&console_lock, flags);
	list_for_each_entry(con, &console_list, next)
		con->write(con, str);
	spin_unlock_irqrestore(&console_lock, flags);
}

/**
 * Adds a character to the console input buffer.
 */
void console_inbuf_add(char c)
{
	unsigned long flags;
	const unsigned short backspace = 0x08;

	spin_lock_irqsave(&console_inbuf_lock, flags);

	if (c == backspace) {
		// Delete a character from the console input buffer
		if (console_inbuf_len > 0)
			--console_inbuf_len;
	} else {
		// Store the character to the console input buffer
		if (console_inbuf_len < CONSOLE_INBUF_SIZE)
			console_inbuf[console_inbuf_len++] = c;
		else
			printk(KERN_WARNING "ERROR: Console input buffer full.\n");
	}

	// Remember that a newline has been encountered
	if (c == '\n')
		console_inbuf_has_newline = true;

	spin_unlock_irqrestore(&console_inbuf_lock, flags);

	// Echo the character to the console
	printk("%c", c);

	// Wakeup anybody waiting for more console input
	waitq_wakeup(&console_inbuf_waitq);
}

/**
 * Reads bytes from the console input buffer.
 */
ssize_t console_inbuf_read(char *buf, size_t len)
{
	unsigned long flags;
	ssize_t n;

	spin_lock_irqsave(&console_inbuf_lock, flags);

	// If the input buffer is empty, wait until the user hits enter.
	while (console_inbuf_len == 0) {
		spin_unlock_irqrestore(&console_inbuf_lock, flags);
		if (wait_event_interruptible(console_inbuf_waitq,
		     (console_inbuf_has_newline == true)))
			return -ERESTARTSYS;
		spin_lock_irqsave(&console_inbuf_lock, flags);
		if (console_inbuf_has_newline == true)
			console_inbuf_has_newline = false;
	}

	// At this point, we know there are characters in the buffer.
	// Return as many characters as possible to the caller.
	n = min(len, console_inbuf_len);
	memcpy(buf, console_inbuf, n);

	// Compact the buffer
	memmove(console_inbuf, console_inbuf + n, console_inbuf_len - n); 
	console_inbuf_len = console_inbuf_len - n;

	spin_unlock_irqrestore(&console_inbuf_lock, flags);
	return n;
}

/**
 * Initializes the console subsystem; called once at boot.
 */
void console_init(void)
{
	driver_init_list("console", console_str);
}

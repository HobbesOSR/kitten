/*
 * Based on the same principle as kgdboe using the NETPOLL api, this
 * driver uses a console polling api to implement a gdb serial inteface
 * which is multiplexed on a console port.
 *
 * Maintainer: Jason Wessel <jason.wessel@windriver.com>
 * for Kitten: Patrick Bridges <bridges@cs.unm.edu>
 *
 * 2007-2008 (c) Jason Wessel - Wind River Systems, Inc.
 * 2008      (c) Patrick Bridges - University of New Mexico
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <lwk/kernel.h>
#include <lwk/types.h>
#include <lwk/ptrace.h>
#include <lwk/kgdb.h>
#include <lwk/console.h>
#include <lwk/driver.h>

static struct kgdb_io		kgdboc_io_ops;

/* -1 = init not run yet, 0 = unconfigured, 1 = configured. */
static int configured		= -1;

static struct console	*kgdb_console_driver;
static int kgdboc_use_con;
static int kgdboc_con_registered;


int kgdboc_serial_register(struct console *p, int *suppress)
{
	int err;

	err = -ENODEV;

	kgdb_console_driver = p;

	err = kgdb_register_io_module(&kgdboc_io_ops);
	if (err)
		goto noconfig;

	configured = 1;
	if (kgdboc_use_con)
		*suppress = 1;

	return 0;

noconfig:
	return err;
}

static int kgdboc_get_char(void)
{
	char c = kgdb_console_driver->poll_get_char(kgdb_console_driver);
	return (int)c;
}

static void kgdboc_put_char(u8 chr)
{
	kgdb_console_driver->poll_put_char(kgdb_console_driver, chr);
}

static struct kgdb_io kgdboc_io_ops = {
	.name			= "kgdboc",
	.read_char		= kgdboc_get_char,
	.write_char		= kgdboc_put_char,
};

void kgdb_console_write(struct console *co, const char *s)
{
	unsigned long flags;

	/* If we're debugging, or KGDB has not connected, don't try
	 * and print. */
	if (!kgdb_connected || atomic_read(&kgdb_active) != -1)
		return;

	local_irq_save(flags);
	kgdb_msg_write(s, strlen(s));
	local_irq_restore(flags);
}

static struct console kgdbcons = {
	.name		= "kgdb",
	.write		= kgdb_console_write,

	/*.flags		= CON_PRINTBUFFER | CON_ENABLED, */
	/*.index		= -1, */
};

int kgdboc_console_register(void)
{
	if (kgdboc_use_con && !kgdboc_con_registered) {
		console_register(&kgdbcons);
		kgdboc_con_registered = 1;
	}
	return 0;
}

int kgdboc_serial_init(void)
{
	driver_init_by_name("console", "serial");
	return 0;
}

int kgdboc_console_init(void)
{
	kgdboc_use_con = 1;
	return 0;
}

DRIVER_INIT("console", kgdboc_console_init); /*  allows console=kgdb */

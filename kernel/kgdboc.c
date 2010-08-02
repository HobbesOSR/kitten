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


#define MAX_CONFIG_LEN		40

static struct kgdb_io		kgdboc_io_ops;

/* -1 = init not run yet, 0 = unconfigured, 1 = configured. */
static int configured		= -1;

static struct console	*kgdb_console_driver;

int kgdboc_serial_register(struct console *p, int *suppress)
{
	int err;
	extern int kgdb_use_con;

	err = -ENODEV;

	kgdb_console_driver = p;

	err = kgdb_register_io_module(&kgdboc_io_ops);
	if (err)
		goto noconfig;

	configured = 1;
	if (kgdb_use_con)
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

static int __init opt_kgdboc_console(char *str)
{
	if (driver_init_by_name("console",str))
                printk(KERN_WARNING "KGDB failed to install console=%s\n", str);
        else
                printk(KERN_INFO "KGDB attached to console=%s\n", str);
	
	if (!configured)
                printk(KERN_WARNING "KGDB console %s failed to initialize IO"
		       " module\n", str);

        return 0;
}

param_func(kgdboc_console, (param_set_fn)opt_kgdboc_console);

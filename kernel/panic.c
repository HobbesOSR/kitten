#include <lwk/kernel.h>
#include <lwk/kallsyms.h>
#include <lwk/kgdb.h>

/**
 * Scream and die.
 */
void panic(const char * fmt, ...)
{
        static char buf[1024];
        va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	printk(KERN_EMERG "Kernel panic: %s\n",buf);

#ifdef CONFIG_KGDB
        printk("Invoking KGDB from panic()...\n");
        kgdb_breakpoint();
#endif

	while (1) {}
}

/**
 * Prints a warning to the console.
 */
void
warn_slowpath(
	const char *		file,
	int			line,
	const char *		fmt, ...
)
{
	va_list args;
	char function[KSYM_SYMBOL_LEN];
	unsigned long caller = (unsigned long)__builtin_return_address(0);
	kallsyms_sprint_symbol(function, caller);

	printk(KERN_WARNING "------------[ cut here ]------------\n");
	printk(KERN_WARNING "WARNING: at %s:%d %s()\n", file, line, function);

	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
}

/* Console Driver when Kitten is running as a multi-instance guest on pisces
 * Jiannan Ouyang (ouyang@cs.pitt.edu)
 */

#include <lwk/delay.h>
#include <lwk/driver.h>
#include <lwk/console.h>
#include <arch/io.h>
#include <lwk/interrupt.h>

#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_lock.h>

extern struct pisces_boot_params * pisces_boot_params;

// Embedded ringbuffer that maps into a 64KB chunk of memory
struct pisces_cons_ringbuf {
	struct pisces_spinlock lock;
    	u64 read_idx;
    	u64 write_idx;
    	u64 cur_len;
    	u8 buf[(64 * 1024) - 32];
} __attribute__((packed));


static struct pisces_cons_ringbuf * console_buffer = NULL;


/** Set when the console has been initialized. */
static int initialized = 0;


/**
 * Prints a single character to the pisces console buffer.
 */
static void pisces_cons_putc(struct console *con, unsigned char c)
{
	pisces_spin_lock(&(console_buffer->lock));

	// If the buffer is full, then we are just going to start overwriting the log
	console_buffer->buf[console_buffer->write_idx] = c;

	console_buffer->cur_len++;
	console_buffer->write_idx++;
	console_buffer->write_idx %= sizeof(console_buffer->buf);
	
	if (console_buffer->cur_len > sizeof(console_buffer->buf)) {
		// We are overwriting, update the read state to be sane
		console_buffer->read_idx++;
		console_buffer->read_idx %= sizeof(console_buffer->buf);
		console_buffer->cur_len--;
	}

	pisces_spin_unlock(&(console_buffer->lock));

}


/**
 * Reads a single character from the pisces console port.
 */
/*
static char pisces_cons_getc(struct console *con)
{
    u64 *cons, *prod;
    char c;

    cons = &console_buffer->in_cons;
    prod = &console_buffer->in_prod;

    pisces_spin_lock(&console_buffer->lock_out);
    c = console_buffer->in[*cons];
    *cons = (*cons + 1) % PISCES_CONSOLE_SIZE_IN; 
    pisces_spin_unlock(&console_buffer->lock_out);


    return c;
}
*/

/**
 * Writes a string to the pisces console buffer.
 */
static void pisces_cons_write(struct console *con, const char *str)
{	
	unsigned char c;
	while ((c = *str++) != '\0') {
		pisces_cons_putc(con, c);
	}
	
}


/**
 * Console device for use by guest instance.
 */
static struct console pisces_console = {
	.name		= "Pisces Guest Console",
	.write		= pisces_cons_write,
//	.poll_get_char	= pisces_cons_getc,
	.poll_put_char	= pisces_cons_putc
};


/**
 * Initializes and registers the pisces console driver.
 */
int pisces_console_init(void) {
	if (initialized) {
		printk(KERN_ERR "Pisces console already initialized.\n");
		return -1;
	}



        console_buffer = __va(pisces_boot_params->console_ring_addr); 

	console_register(&pisces_console);
	initialized = 1;


	return 0;
}


DRIVER_INIT("console", pisces_console_init);
//DRIVER_PARAM(port, uint);

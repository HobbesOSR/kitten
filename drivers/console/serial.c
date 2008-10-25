/* Portions of file (wait_for_xmitr, getc), paraphrased from Linux 8250 
 * driver */

#include <lwk/delay.h>
#include <lwk/driver.h>
#include <lwk/console.h>
#include <lwk/interrupt.h>
#include <arch/io.h>

// Serial port registers
#define TXB		0	// Transmitter Holding Buffer            W
#define RXB		0	// Receiver Buffer                       R
#define DLL		0	// Divisor Latch Low Byte               R/W
#define IER		1	// Interrupt Enable Register            R/W
#define DLH		1	// Divisor Latch High Byte              R/W
#define IIR		2	// Interrupt Identification Register     R
#define FCR		2	// FIFO Control Register                 W
#define LCR		3	// Line Control Register                R/W
#define MCR		4	// Modem Control Register               R/W
#define LSR		5	// Line Status Register                  R
#define MSR		6	// Modem Status Register                 R
#define SCR		7	// Scratch Register                     R/W

// IER bits
#define IER_RLSI	0x08	// RX interrupt
#define IER_THRI	0x02	// TX interrupt
#define IER_RDI		0x01	// Reciver data interrupt

// MCR bits
#define MCR_DTR		0x01	// Enable DTR
#define MCR_RTS		0x02	// Enable RTS
#define MCR_OUT2	0x08	// Enable Out2 (?)

// LSR bits
#define LSR_TXEMPT	0x40	// Empty TX holding register
#define LSR_THREMPT	0x20	// Empty TX holding register
#define LSR_RXRDY	0x01	// Ready to receive

// MSR bits
#define MSR_CTS		0x10	// Clear to send

// LCR bits
#define LCR_DLAB	0x80	// Divisor latch access bit

/** IO port address of the serial port. */
static unsigned int port = 0x3F8;  // COM1

/** Serial port baud rate. */
static unsigned int baud = 9600;
#define SERIAL_MAX_BAUD	115200

#ifdef CONFIG_KGDB
/** Whether to attach to kgdb or the normal console if KGDB is supported */
static int kgdb = 0;
#endif /* CONFIG_KGDB */

/** Set when serial console has been initialized. */
static int initialized = 0;

#define BOTH_EMPTY (LSR_TXEMPT | LSR_THREMPT)
/*
 *      Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(int bits)
{	
        unsigned int status, tmout = 10000;

        /* Wait up to 10ms for the character(s) to be sent. */
        do {
                status = inb(port + LSR);
                if (--tmout == 0)
                        break;
                udelay(1);
        } while ((status & bits) != bits);
}


/**
 * Prints a single character to the serial port.
 */
static void serial_putc(struct console *con, unsigned char c)
{
	// Wait until the TX buffer is empty
	wait_for_xmitr(BOTH_EMPTY);
	// Slam the 8 bits down the 1 bit pipe... meeeooowwwy!
	outb(c, port);
}

/**
 * Writes a string to the serial port.
 */
static void serial_write(struct console *con, const char *str)
{
	unsigned char c;
	while ((c = *str++) != '\0') {
		serial_putc(con, c);
		if (c == '\n')			// new line
			serial_putc(con, '\r');	// tack on carriage return
	}
}

/**
 * Reads a single character from the serial port 
 */
static char serial_getc(struct console *con)
{
	unsigned char lsr = inb_p(port + LSR);
	while (!(lsr & LSR_RXRDY))
		lsr = inb_p(port + LSR);
	return inb_p(port + RXB);
}

/**
 * Serial port console device.
 */
static struct console serial_console = {
	.name  = "Serial Console",
	.write = serial_write,
	.poll_get_char = serial_getc,
	.poll_put_char = serial_putc
};
#ifdef CONFIG_KGDB
int kgdboc_serial_register(struct console *);
#endif

/**
 * Initializes and registers the serial console driver.
 */
void serial_console_init(void)
{
	// Setup the divisor latch registers for the specified baud rate
	unsigned int div = SERIAL_MAX_BAUD / baud;

	if (initialized) {
		printk(KERN_ERR "Serial console already initialized.\n");
		return;
	}

	outb( inb(port+LCR) | LCR_DLAB	, port+LCR ); // set DLAB
	outb( (div>>0) & 0xFF		, port+DLL ); // set divisor low byte
	outb( (div>>8) & 0xFF		, port+DLH ); // set divisor high byte
	outb( inb(port+LCR) & ~LCR_DLAB	, port+LCR ); // unset DLAB
	
	outb( 0x0 , port+IER ); // Disable serial port interrupts
	outb( 0x0 , port+FCR ); // Don't use the FIFOs
	outb( 0x3 , port+LCR ); // 8n1

	// Setup modem control register
	outb( MCR_RTS | MCR_DTR | MCR_OUT2 , port+MCR);

#ifdef CONFIG_KGDB
	if (kgdb)
	    kgdboc_serial_register(&serial_console);
	else
	    console_register(&serial_console);
#else
	console_register(&serial_console);
#endif
	initialized = 1;
}

driver_init(serial_console_init);

/**
 * Configurable parameters for controlling the serial port
 * I/O port address and baud.
 */
driver_param(port, uint);
driver_param(baud, uint);
#ifdef CONFIG_KGDB
driver_param(kgdb, uint);
#endif

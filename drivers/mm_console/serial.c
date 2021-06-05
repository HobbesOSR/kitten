/* Portions of file (wait_for_xmitr, getc), paraphrased from Linux 8250 
 * driver */

#include <lwk/kernel.h>
#include <lwk/delay.h>
#include <lwk/driver.h>
#include <lwk/console.h>
#include <lwk/interrupt.h>
#include <arch/memory.h>
#include <arch/io.h>


static uint32_t* addr = NULL;

#define UART_PHYS_ADDR _AC(CONFIG_SERIAL_PHYS,u)

//#define UART_LCR    (UART_BASE_ADDR + 0xC)
//#define UART_DLL    (UART_BASE_ADDR + 0x0)
//#define UART_DLM    (UART_BASE_ADDR + 0x4)
//#define UART_FCR    (UART_BASE_ADDR + 0x8)
//#define UART_IER    (UART_BASE_ADDR + 0x4)
//#define UART_THR    (UART_BASE_ADDR + 0x0)
//#define UART_LSR    (UART_BASE_ADDR + 0x14)
//#define UART_RBR    (UART_BASE_ADDR + 0x0)

// Serial port registers
//#define TXB		0	// Transmitter Holding Buffer            W
#define RXB		0x0u		// Receiver Buffer                       R
//#define DLL		0		// Divisor Latch Low Byte               R/W
//#define IER		1		// Interrupt Enable Register            R/W
//#define DLH		1		// Divisor Latch High Byte              R/W
//#define IIR		2		// Interrupt Identification Register     R
//#define FCR		2		// FIFO Control Register                 W
//#define LCR		3		// Line Control Register                R/W
//#define MCR		4		// Modem Control Register               R/W
#define LSR		0x14u	// Line Status Register                  R
//#define MSR		6	// Modem Status Register                 R
//#define SCR		7	// Scratch Register                     R/W

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

// MSR bitsLSR_TXEMPT
#define MSR_CTS		0x10	// Clear to send

// LCR bits
#define LCR_DLAB	0x80	// Divisor latch access bit

/** IO port address of the serial port. */
static volatile u8* port = EARLYCON_IOBASE + (UART_PHYS_ADDR & ((1u << PMD_SHIFT) - 1));  // COM1

/** Serial port baud rate. */
static unsigned int baud = 115200;
#define SERIAL_MAX_BAUD	115200

/** Set when serial console has been initialized. */
static int initialized = 0;

// TODO: Brian
#define BOTH_EMPTY (LSR_TXEMPT | LSR_THREMPT)

/* Over ride in/out to do memory writes */
#define inb(addr) \
	((u8)*((volatile u8*)addr))

#define outb(ch, addr) \
		*((volatile u8*)(addr)) = ch

/*
 *      Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(int bits)
{	
        unsigned int status, tmout = 10000u;

        /* Wait up to 10ms for the character(s) to be sent. */
        do {
                /*if (--tmout == 0)
                        break;*/
                //while(tmout-- !=0);//udelay(1);
        } while ((inb(port + LSR) & bits) != bits);
}

void
serial_num(unsigned long long num)
{
	char* digits = "0123456789abcdef";
	char str[18];
	int i = 16;
	memset(str,0,18);
	str[i] = '\n';
	do {
		i--;
		str[i] = digits[num % 16];
		num = num/16;
	} while (i != 0);
	serial_write(NULL, str);

}

/**
 * Prints a single character to the serial port.
 */
/*
 * static u8
get_status( void )
{
	return *(u8* volatile)UART_LSR;
}

void
put_char(u8 ch)
{
	while((get_status() & 0x20) == 0x0);
	*((u8* volatile)UART_THR) = ch;
}
 */
static void serial_putc(struct console *con, unsigned char c)
{
// Wait until the TX buffer is empty
	// TODO Brian temporarily commented out next line while to solve slow serial issue.
	wait_for_xmitr(LSR_THREMPT);
	// Slam the 8 bits down the 1 bit pipe... meeeooowwwy!
	outb(c, port);
}

/**
 * Writes a string to the serial port.
 */
void serial_write(struct console *con, const char *str)
{
	unsigned char c;

	while ((c = *str++) != '\0') {
		serial_putc(con, c);
		if (c == '\n')	// new line
			serial_putc(con, '\r');	// tack on carriage return
	}
	asm volatile("dsb #0\n");
}

/**
 * Reads a single character from the serial port 
 */
static char serial_getc(struct console *con)
{
	unsigned char lsr = inb(port + LSR);
	while (!(lsr & LSR_RXRDY))
		lsr = inb(port + LSR);
	return inb(port + RXB);
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
#ifdef CONFIG_KGDB_SERIAL_CONSOLE
int kgdboc_serial_register(struct console *, int *suppress);
#endif

/**
 * Initializes and registers the serial console driver.
 */
int serial_console_init(void)
{
	// Setup the divisor latch registers for the specified baud rate
	unsigned int div = SERIAL_MAX_BAUD / baud;
	int suppress = 0;

	if (initialized) {
		printk(KERN_ERR "Serial console already initialized.\n");
		return -1;
	}

	addr = EARLYCON_IOBASE + (UART_PHYS_ADDR & ((1u << PMD_SHIFT) - 1));
	//while()
	port = addr;

/*
	outb( inb(port+LCR) | LCR_DLAB	, port+LCR ); // set DLAB
	outb( (div>>0) & 0xFF		, port+DLL ); // set divisor low byte
	outb( (div>>8) & 0xFF		, port+DLH ); // set divisor high byte
	outb( inb(port+LCR) & ~LCR_DLAB	, port+LCR ); // unset DLAB
	
	outb( 0x0 , port+IER ); // Disable serial port interrupts
	outb( 0x0 , port+FCR ); // Don't use the FIFOs
	outb( 0x3 , port+LCR ); // 8n1

	// Setup modem control register
	outb( MCR_RTS | MCR_DTR | MCR_OUT2 , port+MCR);

#ifdef CONFIG_KGDB_SERIAL_CONSOLE
	kgdboc_serial_register(&serial_console, &suppress);
#endif
*/
	if (!suppress)
		console_register(&serial_console);

	initialized = 1;

	return 0;
}

void* get_port()
{
	return port;
}

DRIVER_INIT("console", serial_console_init);

/**
 * Configurable parameters for controlling the serial port
 * I/O port address and baud.
 */
DRIVER_PARAM(port, uint);
DRIVER_PARAM(baud, uint);

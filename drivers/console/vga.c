#include <lwk/types.h>
#include <lwk/string.h>
#include <lwk/console.h>
#include <lwk/params.h>

/** Base address of the VGA frame buffer. */
static volatile uint8_t * const vga_fb = (uint8_t *) 0xffffffff800b8000ul;

/** Current cursor row coordinate. */
static int row = 0;
DRIVER_PARAM(row, int,
	"Starting screen row. Example: vga.row=0");

/** Current cursor column coordinate. */
static int col = 0;

/** Number of rows on the screen. */
static int nrows = 25;

/** Number of columns on the screen. */
static int ncols = 80;

/** Set when vga console has been initialized. */
static int initialized = 0;


/** Calculates the offset in the vga_fb corresponding to (row, col). */
static inline int
cursor( int row, int col )
{
	return (row * ncols * 2) + col * 2;
}


/** Scrolls everything on the screen up by one row. */
static void
vga_scroll( void )
{
	int i;

	// Move all existing lines up by one
	memmove(
		(void *) vga_fb,
		(void *) (vga_fb + cursor(1, 0)),
		(nrows - 1) * ncols * sizeof(uint16_t)
	);

	// Blank the new line at the bottom of the screen
	for (i = 0; i < ncols; i++)
		vga_fb[cursor(nrows-1, i)] = ' ';
}


/** Moves cursor to the next line. */
static void
vga_newline( void )
{
	row = row + 1;
	col = 0;

	if (row == nrows) {
		row = nrows - 1;
		vga_scroll();
	}
}


/** Sets the VGA font color. */
static void
vga_set_font_color( uint8_t color )
{
	int i, j;
	for (i = 0; i < nrows; i++)
		for (j = 0; j < ncols; j++)
			vga_fb[cursor(i, j) + 1] = color;
}


/** Prints a single character to the screen. */
static void
vga_putc( unsigned char c )
{
	// Print the character
	vga_fb[cursor(row, col)] = c;

	// Move cursor
	if (++col == ncols)
		vga_newline();
}


/** Writes a string to the screen at the current cursor location. */
static void
vga_write(
	struct console *	con,
	const char *		str
)
{
	unsigned char c;

	while ((c = *str++) != '\0') {
		switch (c) {
			case '\n':
				vga_newline();
				break;

			case '\t':
				/* Emulate a TAB */
				vga_putc(' ');
				while ((col % 8) != 0)
					vga_putc(' ');
				break;

			default: 
				vga_putc(c);
		}
	}
}


/** VGA console device. */
static struct console vga_console = {
	.name = "VGA Console",
	.write = vga_write
};


/** Initializes and registers the VGA console driver. */
void
vga_console_init( void )
{
	if (initialized) {
		printk(KERN_ERR "VGA console already initialized.\n");
		return;
	}

	vga_set_font_color(0x0F /* White */);
	console_register(&vga_console);
	initialized = 1;
}


REGISTER_CONSOLE_DRIVER(vga, vga_console_init);


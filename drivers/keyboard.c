/** \file
 * Interface to standard PC keyboards.
 *
 * It is in device class "late", which is done after most every other
 * device has been setup.
 */

/*
 * Support for passing keyboard data to the console buffer added by:
 * Jedidiah R. McClurg
 * Northwestern University
 * 4-18-2012
 *
 * The keycode processing functionality is adapted from GeekOS,
 * whose authors have allowed their code to be used with the
 * stipulation that the following notice is posted:
 *
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <lwk/driver.h>
#include <lwk/signal.h>
#include <lwk/interrupt.h>
#include <lwk/console.h>
#include <arch/proto.h>
#include <arch/io.h>
#include <arch/idt_vectors.h>

/*
 * Flags
 */
#define KEY_SPECIAL_FLAG 0x0100
#define KEY_KEYPAD_FLAG  0x0200
#define KEY_SHIFT_FLAG   0x1000
#define KEY_ALT_FLAG     0x2000
#define KEY_CTRL_FLAG    0x4000
#define KEY_RELEASE_FLAG 0x8000

/*
 * Special key codes
 */
#define _SPECIAL(num) (KEY_SPECIAL_FLAG | (num))
#define KEY_UNKNOWN   _SPECIAL(0)
#define KEY_F1        _SPECIAL(1)
#define KEY_F2        _SPECIAL(2)
#define KEY_F3        _SPECIAL(3)
#define KEY_F4        _SPECIAL(4)
#define KEY_F5        _SPECIAL(5)
#define KEY_F6        _SPECIAL(6)
#define KEY_F7        _SPECIAL(7)
#define KEY_F8        _SPECIAL(8)
#define KEY_F9        _SPECIAL(9)
#define KEY_F10       _SPECIAL(10)
#define KEY_F11       _SPECIAL(11)
#define KEY_F12       _SPECIAL(12)
#define KEY_LCTRL     _SPECIAL(13)
#define KEY_RCTRL     _SPECIAL(14)
#define KEY_LSHIFT    _SPECIAL(15)
#define KEY_RSHIFT    _SPECIAL(16)
#define KEY_LALT      _SPECIAL(17)
#define KEY_RALT      _SPECIAL(18)
#define KEY_PRINTSCRN _SPECIAL(19)
#define KEY_CAPSLOCK  _SPECIAL(20)
#define KEY_NUMLOCK   _SPECIAL(21)
#define KEY_SCRLOCK   _SPECIAL(22)
#define KEY_SYSREQ    _SPECIAL(23)

/*
 * Keypad keys
 */
#define KEYPAD_START  128
#define _KEYPAD(num)  (KEY_KEYPAD_FLAG | KEY_SPECIAL_FLAG | (num+KEYPAD_START))
#define KEY_KPHOME    _KEYPAD(0)
#define KEY_KPUP      _KEYPAD(1)
#define KEY_KPPGUP    _KEYPAD(2)
#define KEY_KPMINUS   _KEYPAD(3)
#define KEY_KPLEFT    _KEYPAD(4)
#define KEY_KPCENTER  _KEYPAD(5)
#define KEY_KPRIGHT   _KEYPAD(6)
#define KEY_KPPLUS    _KEYPAD(7)
#define KEY_KPEND     _KEYPAD(8)
#define KEY_KPDOWN    _KEYPAD(9)
#define KEY_KPPGDN    _KEYPAD(10)
#define KEY_KPINSERT  _KEYPAD(11)
#define KEY_KPDEL     _KEYPAD(12)

/*
 * ASCII codes for which there is no convenient C representation
 */
#define ASCII_ESC 0x1B
#define ASCII_BS  0x08

/*
 * Keycode type
 */
typedef unsigned short Keycode;

/*
 * Translate from scan code to key code, when shift is not pressed.
 */
static const Keycode codesUnshift[] = {
	KEY_UNKNOWN, ASCII_ESC, '1', '2',                   /* 0x00 - 0x03 */
	'3', '4', '5', '6',                                 /* 0x04 - 0x07 */
	'7', '8', '9', '0',                                 /* 0x08 - 0x0B */
	'-', '=', ASCII_BS, '\t',                           /* 0x0C - 0x0F */
	'q', 'w', 'e', 'r',                                 /* 0x10 - 0x13 */
	't', 'y', 'u', 'i',                                 /* 0x14 - 0x17 */
	'o', 'p', '[', ']',                                 /* 0x18 - 0x1B */
	'\n', KEY_LCTRL, 'a', 's',                          /* 0x1C - 0x1F */
	'd', 'f', 'g', 'h',                                 /* 0x20 - 0x23 */
	'j', 'k', 'l', ';',                                 /* 0x24 - 0x27 */
	'\'', '`', KEY_LSHIFT, '\\',                        /* 0x28 - 0x2B */
	'z', 'x', 'c', 'v',                                 /* 0x2C - 0x2F */
	'b', 'n', 'm', ',',                                 /* 0x30 - 0x33 */
	'.', '/', KEY_RSHIFT, KEY_PRINTSCRN,                /* 0x34 - 0x37 */
	KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1,                /* 0x38 - 0x3B */
	KEY_F2, KEY_F3, KEY_F4, KEY_F5,                     /* 0x3C - 0x3F */
	KEY_F6, KEY_F7, KEY_F8, KEY_F9,                     /* 0x40 - 0x43 */
	KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,      /* 0x44 - 0x47 */
	KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,      /* 0x48 - 0x4B */
	KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,   /* 0x4C - 0x4F */
	KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,    /* 0x50 - 0x53 */
	KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,  /* 0x54 - 0x57 */
};

/*
 * Translate from scan code to key code, when shift *is* pressed.
 * Keep this in sync with the unshifted table above!
 * They must be the same size.
 */
static const Keycode codesShift[] = {
    KEY_UNKNOWN, ASCII_ESC, '!', '@',                       /* 0x00 - 0x03 */
    '#', '$', '%', '^',                                     /* 0x04 - 0x07 */
    '&', '*', '(', ')',                                     /* 0x08 - 0x0B */
    '_', '+', ASCII_BS, '\t',                               /* 0x0C - 0x0F */
    'Q', 'W', 'E', 'R',                                     /* 0x10 - 0x13 */
    'T', 'Y', 'U', 'I',                                     /* 0x14 - 0x17 */
    'O', 'P', '{', '}',                                     /* 0x18 - 0x1B */
    '\n', KEY_LCTRL, 'A', 'S',                              /* 0x1C - 0x1F */
    'D', 'F', 'G', 'H',                                     /* 0x20 - 0x23 */
    'J', 'K', 'L', ':',                                     /* 0x24 - 0x27 */
    '"', '~', KEY_LSHIFT, '|',                              /* 0x28 - 0x2B */
    'Z', 'X', 'C', 'V',                                     /* 0x2C - 0x2F */
    'B', 'N', 'M', '<',                                     /* 0x30 - 0x33 */
    '>', '?', KEY_RSHIFT, KEY_PRINTSCRN,                    /* 0x34 - 0x37 */
    KEY_LALT, ' ', KEY_CAPSLOCK, KEY_F1,                    /* 0x38 - 0x3B */
    KEY_F2, KEY_F3, KEY_F4, KEY_F5,                         /* 0x3C - 0x3F */
    KEY_F6, KEY_F7, KEY_F8, KEY_F9,                         /* 0x40 - 0x43 */
    KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_KPHOME,          /* 0x44 - 0x47 */
    KEY_KPUP, KEY_KPPGUP, KEY_KPMINUS, KEY_KPLEFT,          /* 0x48 - 0x4B */
    KEY_KPCENTER, KEY_KPRIGHT, KEY_KPPLUS, KEY_KPEND,       /* 0x4C - 0x4F */
    KEY_KPDOWN, KEY_KPPGDN, KEY_KPINSERT, KEY_KPDEL,        /* 0x50 - 0x53 */
    KEY_SYSREQ, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,      /* 0x54 - 0x57 */
};

#define LEFT_SHIFT  0x01
#define RIGHT_SHIFT 0x02
#define LEFT_CTRL   0x04
#define RIGHT_CTRL  0x08
#define LEFT_ALT    0x10
#define RIGHT_ALT   0x20
#define SHIFT_MASK  (LEFT_SHIFT | RIGHT_SHIFT)
#define CTRL_MASK   (LEFT_CTRL | RIGHT_CTRL)
#define ALT_MASK    (LEFT_ALT | RIGHT_ALT)


/**
 * Handles a keyboard key press.
 */
static char
handle_key(uint8_t key)
{
	static unsigned int mask;
	bool release = (key & 0x80);  // is this a key release?
	key &= 0x7f;  // remove the "release" flag if it's set
	unsigned flag = 0;

	// Process the key
	unsigned keycode = ((mask & SHIFT_MASK) != 0) ? codesShift[key] : codesUnshift[key];

	switch (keycode) {
		case KEY_LSHIFT:
			flag = LEFT_SHIFT;
			break;
		case KEY_RSHIFT:
			flag = RIGHT_SHIFT;
			break;
		case KEY_LCTRL:
			flag = LEFT_CTRL;
			break;
		case KEY_RCTRL:
			flag = RIGHT_CTRL;
			break;
		case KEY_LALT:
			flag = LEFT_ALT;
			break;
		case KEY_RALT:
			flag = RIGHT_ALT;
			break;
		default:
			// if this a keypress, and the key is not a special key,
			// return the character
			if (!release && !(keycode & KEY_SPECIAL_FLAG))
				return keycode;
			else
				return 0;
	}

	// update the current mask status
	if (release)
		mask &= ~(flag);
	else
		mask |= flag;

	// return 0 if it's a special key
	return 0;
}


/**
 * Keyboard interrupt handler.
 */
static irqreturn_t
do_keyboard_interrupt(
	int			vector,
	void *			unused
)
{
	const uint8_t KB_STATUS_PORT = 0x64;
	const uint8_t KB_DATA_PORT   = 0x60;
	const uint8_t KB_OUTPUT_FULL = 0x01;

	uint8_t status = inb(KB_STATUS_PORT);

	if ((status & KB_OUTPUT_FULL) == 0)
		return IRQ_NONE;

	uint8_t key = inb(KB_DATA_PORT);
	//printk("Keyboard Interrupt: status=%u, key=%u\n", status, key);

	char c = handle_key(key);
	if (c != 0)
		console_inbuf_add(c);

	return IRQ_HANDLED;
}


/**
 * Initializes the keyboard driver.
 * Called once at boot time.
 */
int
keyboard_init(void)
{
	irq_request(
		IRQ1_VECTOR,
		&do_keyboard_interrupt,
		0,
		"keyboard",
		NULL
	);

	return 0;
}


DRIVER_INIT("late", keyboard_init);

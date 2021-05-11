/*
 * The Minimal snprintf() implementation
 *
 * Copyright (c) 2013,2014 Michal Ludvig <michal@logix.cz>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the auhor nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ----
 *
 * This is a minimal snprintf() implementation optimised
 * for embedded systems with a very limited program memory.
 * mini_snprintf() doesn't support _all_ the formatting
 * the glibc does but on the other hand is a lot smaller.
 * Here are some numbers from my STM32 project (.bin file size):
 *      no snprintf():      10768 bytes
 *      mini snprintf():    11420 bytes     (+  652 bytes)
 *      glibc snprintf():   34860 bytes     (+24092 bytes)
 * Wasting nearly 24kB of memory just for snprintf() on
 * a chip with 32kB flash is crazy. Use mini_snprintf() instead.
 *
 */
#include <stdarg.h>

#include <lwk/types.h>

#include <arch/memory.h>


static u32
__strlen(const char * s)
{
	unsigned int len = 0;

	while (s[len] != '\0') {
		len++;
	}
    
	return len;
}

static unsigned int
__itoa(int            value,
       unsigned int   radix,
       unsigned int   uppercase,
       unsigned int   unsig,
       char         * buffer,
       unsigned int   zero_pad)
{
	char       * pbuffer  = buffer;
	int	     negative = 0;
	unsigned int i;
	unsigned int len;

	/* No support for unusual radixes. */
	if (radix > 16) {
		return 0;
	}

	if ( (value  < 0) &&
	     (unsig == 0) ) {
	
		negative = 1;
		value    = -value;
	}

	/* This builds the string back to front ... */
	do {
		int digit = value % radix;

		if (digit < 10) {
			*(pbuffer++) = ('0' + digit);
		} else {
			*(pbuffer++) = (uppercase ? 'A' : 'a') + digit - 10;
		}
	
		value /= radix;

	} while (value > 0);

	for (i = (pbuffer - buffer); i < zero_pad; i++) {
		*(pbuffer++) = '0';
	}

	if (negative) {
		*(pbuffer++) = '-';
	}
    
	*(pbuffer) = '\0';

	/* ... now we reverse it (could do it recursively but will
	 * conserve the stack space) */
	len = (pbuffer - buffer);
    
	for (i = 0; i < len / 2; i++) {
		char j = buffer[i];
	
		buffer[i]           = buffer[len - i - 1];
		buffer[len - i - 1] = j;
	}

	return len;
}


static unsigned int
__lltoa(long long      value,
        unsigned int   radix,
        unsigned int   uppercase,
        unsigned int   unsig,
        char         * buffer,
        unsigned int   zero_pad)
{
	char       * pbuffer  = buffer;
	int	     negative = 0;
	u64          tmp_val  = 0; 
	unsigned int i;
	unsigned int len;

	/* No support for unusual radixes. */
	if (radix > 16) {
		return 0;
	}

	if ( (value  < 0) &&
	     (unsig == 0) ) {
	
		negative = 1;
		tmp_val    = -value;
	} else {
		tmp_val = value;
	}

	/* This builds the string back to front ... */
	do {		
		int digit = tmp_val % radix;
		
		if (digit < 10) {
			*(pbuffer++) = ('0' + digit);
		} else {
			*(pbuffer++) = (uppercase ? 'A' : 'a') + digit - 10;
		}
	
		tmp_val /= radix;

	} while (tmp_val > 0);

	for (i = (pbuffer - buffer); i < zero_pad; i++) {
		*(pbuffer++) = '0';
	}

	if (negative) {
		*(pbuffer++) = '-';
	}
    
	*(pbuffer) = '\0';

	/* ... now we reverse it (could do it recursively but will
	 * conserve the stack space) */
	len = (pbuffer - buffer);
    
	for (i = 0; i < len / 2; i++) {
		char j = buffer[i];
	
		buffer[i]           = buffer[len - i - 1];
		buffer[len - i - 1] = j;
	}

	return len;
}



static int
__putc(char ch) {
	*(volatile uint8_t *)EARLYCON_IOBASE = ch;
        asm ("":::"memory");
	return (int)ch;
}

static int
__puts(char * str)
{
	while (*str) {
		__putc(*str);
		str++;
	}

	return 0;
}

static int
__vprintf(const char   * fmt,
          va_list        va)
{
	char bf[24];
	char ch;


	while ((ch = *(fmt++))) {

	
		if (ch != '%') {
			__putc(ch);
		} else {
			char   zero_pad = 0;
			char * ptr      = NULL;
			u8     ll_mode  = 0;
	    
			ch = *(fmt++);

			/* Zero padding requested */
			if (ch == '0') {

				ch = *(fmt++);

				if (ch == '\0') {
					goto end;
				}
		
				if ( (ch >= '0') &&
				     (ch <= '9') ) {
					zero_pad = ch - '0';
				}
		
				ch = *(fmt++);
			}

			if (ch == 'l') {
				ch = *(fmt++);

				if (ch == 'l') {
					ch = *(fmt++);
					ll_mode = 1;
				}		
			}
	    
			switch (ch) {
				case 0:
					goto end;

				case 'u':
				case 'd':
					if (ll_mode == 0) {
						__itoa(va_arg(va, u32), 10, 0, (ch == 'u'), bf, zero_pad);
					} else {
						__lltoa(va_arg(va, u64), 10, 0, (ch == 'u'), bf, zero_pad);
					}
		    
					__puts(bf);
					break;

				case 'x':
				case 'X':
					if (ll_mode == 0) {
						__itoa(va_arg(va, u32), 16, (ch == 'X'), 1, bf, zero_pad);
					} else {
						__lltoa(va_arg(va, u64), 16, (ch == 'X'), 1, bf, zero_pad);
					}

					__puts(bf);
					break;

				case 'p':
					__lltoa(va_arg(va, u64), 16, 0, 1, bf, 16);

					__puts(bf);
					break;
				case 'c' :
					__putc((char)(va_arg(va, int)));
					break;

				case 's' :
					ptr = va_arg(va, char*);
		    
					__puts(ptr);

					break;

				default:
					__putc(ch);
					break;
			}
		}
	}
    
 end:
	return 0;
}


int
early_printk(const char * fmt, ...)
{
	int ret;
	va_list va;
	va_start(va, fmt);
	ret = __vprintf(fmt, va);
	va_end(va);

	return ret;
}
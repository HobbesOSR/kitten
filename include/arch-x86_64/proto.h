#ifndef _X86_64_PROTO_H
#define _X86_64_PROTO_H

/* misc architecture specific prototypes */

extern void early_idt_handler(void);

/* Driver init prototypes... TODO: move these */
extern void vga_console_init(int);
extern void serial_console_init(void);
extern void l0_console_init(void);

#endif

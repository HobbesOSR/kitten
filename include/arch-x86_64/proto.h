#ifndef _X86_64_PROTO_H
#define _X86_64_PROTO_H

/* misc architecture specific prototypes */

extern void early_idt_handler(void);

extern void vga_init(int);
extern void serial_init(void);

#endif

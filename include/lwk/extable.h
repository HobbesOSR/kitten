#ifndef _LWK_EXTABLE_H
#define _LWK_EXTABLE_H

#include <lwk/init.h>
#include <arch/extable.h>

extern void __init
sort_exception_table(void);

extern const struct exception_table_entry *
search_exception_table(unsigned long);

const struct exception_table_entry *
search_extable(const struct exception_table_entry *first,
               const struct exception_table_entry *last,
               unsigned long value);

void
sort_extable(struct exception_table_entry *start,
             struct exception_table_entry *finish);

#endif

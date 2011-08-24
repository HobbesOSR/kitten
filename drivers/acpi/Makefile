#
# Makefile for the ACPI interpreter
#

export ACPI_CFLAGS
ACPI_CFLAGS := -Os
ifdef CONFIG_ACPI_DEBUG
	ACPI_CFLAGS += -DACPI_DEBUG_OUTPUT
endif
EXTRA_CFLAGS += $(ACPI_CFLAGS)

#
# ACPI Boot-Time Table Parsing
#
obj-y				+= tables.o

#
# ACPI Core Subsystem (Interpreter)
#
obj-y				+= acpi.o \
					acpica/

# All the builtin files are in the "acpi." module_param namespace.
acpi-y				+= osl.o

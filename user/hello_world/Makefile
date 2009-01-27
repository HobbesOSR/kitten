TARGET = hello_world

# Default to building in the current directory, unless
# overridden by the calling Makefile or on the command line.
O=$(shell pwd)


PROGS-y += $(TARGET)
OBJS-y += hello_world.o

INCLUDES += \
	-I../liblwk/include \
	-I../../include \
	-I$O/../../include2 \


$O/$(TARGET): $(addprefix $O/,$(OBJS-y) ../liblwk/liblwk.a)
	$(call buildprog,$@,$^ -lpthread)

include ../Make.rules

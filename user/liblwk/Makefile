TARGET = liblwk.a

LIBS-y += $(TARGET)
OBJS-y += syscalls.o pmem.o elf.o aspace.o

INCLUDES += \
	-Iinclude \
	-I../../include \
	-I$O/../../include2 \

$O/$(TARGET): $(addprefix $O/,$(OBJS-y))
	$(call buildlib,$@,$^)

include ../Make.rules

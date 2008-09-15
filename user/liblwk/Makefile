TARGET = liblwk.a

LIBS-y += $(TARGET)
OBJS-y += syscalls.o pmem.o elf.o

INCLUDES += -I./include -I../../include

$(TARGET): $(OBJS-y)
	$(call buildlib,$@,$(OBJS-y))

include ../Make.rules

TARGET = hello_world

PROGS-y += $(TARGET)
OBJS-y += hello_world.o

INCLUDES += -I../liblwk/include -I../../include

$(TARGET): $(OBJS-y)
	$(call buildprog,$@,$(OBJS-y) ../liblwk/liblwk.a)

include ../Make.rules

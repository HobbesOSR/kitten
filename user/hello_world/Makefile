TARGET = hello_world

PROGS-y += $(TARGET)
OBJS-y += hello_world.o

INCLUDES += \
	-I../liblwk/include \
	-I../../include \
	-I$O/../../include2 \


$O/$(TARGET): $(addprefix $O/,$(OBJS-y) ../liblwk/liblwk.a)
	$(call buildprog,$@,$^ -lpthread)

include ../Make.rules

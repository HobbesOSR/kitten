# Any .o files dropped in here will be linked into the kernel.
# This is a primitive way to allow external builds of complex
# code, such as Infiniband or Linux compatability libraries.

# Get all the .o files except for built-in.o
object_files:=$(subst $(obj)/built-in.o,,$(wildcard $(obj)/*.o))
lib_files:=$(subst $(obj)/lib.a,,$(wildcard $(obj)/*.a))

# Strip the object file directory name
obj-y := $(notdir $(object_files)) null.o
lib-y := $(notdir $(lib_files))

# Override the rules to build the modules that are linked in
# to avoid trying to build them.  The message should never
# be printed.
$(object_files) $(lib_files): FORCE
	@echo >&2 "  MOD     $@"

FORCE:

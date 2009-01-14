# Any .o files dropped in here will be linked into the kernel.
# This is a primitive way to allow external builds of complex
# code, such as Infiniband or Linux compatability libraries.

# Get all the .o files except for built-in.o
object_files:=$(subst built-in.o,,$(wildcard $(obj)/*.o))

# Strip the object file directory name
obj-y := $(notdir $(object_files)) null.o

# Override the rules to build the modules that are linked in
# to avoid trying to build them.  The message should never
# be printed.
$(object_files):
	@echo >&2 " NOTICE   $@"

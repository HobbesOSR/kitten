LDFLAGS = 
LDFLAGS += -L$(OPENIB)/lib 
LDFLAGS += -L../libwait4net
LDFLAGS += -L../libmalloc
LDFLAGS += -L../libinit_ompi
LDFLAGS += -L../libarp
LDFLAGS += -L../../liblwk
LDFLAGS += -Wl,-whole-archive -lmalloc -Wl,-no-whole-archive
LDFLAGS	+= -Wl,-whole-archive -lmlx4 -Wl,-no-whole-archive
LDFLAGS += -Wl,-whole-archive -lwait4net -Wl,-no-whole-archive
LDFLAGS += -Wl,-whole-archive -linitArp -Wl,-no-whole-archive
LDFLAGS += -Wl,-whole-archive -linit_ompi -Wl,-no-whole-archive
LDFLAGS += -lrdmacm
LDFLAGS += -libverbs
LDFLAGS += -llwk
LDFLAGS += -lpthread

CPPFLAGS = -Wall
CPPFLAGS += -Iinclude
CPPFLAGS += -I../librdmaPlus
CPPFLAGS += -I$(OPENIB)/include
CPPFLAGS += -I../../../include
CPPFLAGS += -I../liblwkOrte
CPPFLAGS += -I$(OPENMPI_SRC)
CPPFLAGS += -I$(OPENMPI_SRC)/build/orte/include
CPPFLAGS += -I$(OPENMPI_SRC)/build/opal/include
CPPFLAGS += -I$(OPENMPI_SRC)/orte/include
CPPFLAGS += -I$(OPENMPI_SRC)/opal/include


pct_SRC = main.cpp pct.cpp job.cpp app.cpp pmem.cpp mem.cpp orte.cpp dl.cpp

pct_OBJS = $(pct_SRC:.cpp=.o)

all: pct

pct: $(pct_OBJS) 
	$(CXX) -static $(pct_OBJS) $(LDFLAGS) -o $@

clean:
	rm -rf $(pct_OBJS) pct

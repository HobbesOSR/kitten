OPENMPI=/home/mjleven/hg3/kitten/user/libs/openmpi-1.2.9/

LDFLAGS = 
LDFLAGS += -L$(HOME)/IB/install/lib 
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
CPPFLAGS += -I$(HOME)/IB/install/include
CPPFLAGS += -I../../../include
CPPFLAGS += -I../liblwkOrte
CPPFLAGS += -I$(OPENMPI)/build/opal/include
CPPFLAGS += -I$(OPENMPI)/build/orte/include
CPPFLAGS += -I$(OPENMPI)/build/ompi/include
CPPFLAGS += -I$(OPENMPI)/orte/mca/rml/cnos
CPPFLAGS += -I$(OPENMPI)/opal/include
CPPFLAGS += -I$(OPENMPI)/orte/include
CPPFLAGS += -I$(OPENMPI)/ompi/include
CPPFLAGS += -I$(OPENMPI)

pct_SRC = main.cpp pct.cpp job.cpp app.cpp pmem.cpp mem.cpp orte.cpp dl.cpp

pct_OBJS = $(pct_SRC:.cpp=.o)

all: pct

pct: $(pct_OBJS) 
	$(CXX) -static $(pct_OBJS) $(LDFLAGS) -o $@

clean:
	rm -rf $(pct_OBJS) pct

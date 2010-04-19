
LDFLAGS = 
LDFLAGS += -L$(HOME)/IB/install/lib 
LDFLAGS += -L$(HOME)/local/lib 
LDFLAGS += -L../libsrdma/src
LDFLAGS += -L../libwait4net
LDFLAGS += -L../libinit_dapl
LDFLAGS += -L../libinit_ompi
LDFLAGS += -L../libarp
LDFLAGS += -L../../liblwk
LDFLAGS	+= -Wl,-whole-archive -lmlx4 -Wl,-no-whole-archive
LDFLAGS += -Wl,-whole-archive -lwait4net -Wl,-no-whole-archive
LDFLAGS += -Wl,-whole-archive -linitArp -Wl,-no-whole-archive
LDFLAGS += -Wl,-whole-archive -linit_dapl -Wl,-no-whole-archive
LDFLAGS += -Wl,-whole-archive -linit_ompi -Wl,-no-whole-archive
LDFLAGS += -Wl,-whole-archive -ldaplofa -Wl,-no-whole-archive
LDFLAGS += -lsrdma
LDFLAGS += -ldat2
LDFLAGS += -lrdmacm
LDFLAGS += -libverbs
LDFLAGS += -llwk
LDFLAGS += -lpthread

CPPFLAGS = -Wall
CPPFLAGS += -Iinclude
CPPFLAGS += -I../libsrdma/include
CPPFLAGS += -I$(HOME)/IB/install/include
CPPFLAGS += -I../../../include

DEP=../libsrdma/src/libsrdma.a ../libinit_dapl/libinit_dapl.a

pct_SRC = main.cpp pct.cpp job.cpp app.cpp pmem.cpp mem.cpp dl.cpp

pct_OBJS = $(pct_SRC:.cpp=.o)

all: pct

pct: $(DEP) $(pct_OBJS) 
	$(CXX) -static $(pct_OBJS) $(LDFLAGS) -o $@

test: $(DEP) test.o 
	$(CXX) -static test.o dl.o $(LDFLAGS) -o $@

clean:
	rm -rf $(pct_OBJS) pct  test.o test

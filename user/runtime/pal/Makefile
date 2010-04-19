
LDFLAGS =
LDFLAGS += -L$(HOME)/IB/install/lib
LDFLAGS += -L../libsrdma/src

LDFLAGS += -lrdmacm
LDFLAGS += -lsrdma
LDFLAGS += -libverbs
LDFLAGS += -ldat2
LDFLAGS += -lpthread
LDFLAGS += -ldl

CPPFLAGS = -Wall
CPPFLAGS += -Iinclude
CPPFLAGS += -I../libsrdma/include
CPPFLAGS += -I../pct/include
CPPFLAGS += -I$(HOME)/IB/install/include

all: pal

DEP=../libsrdma/src/libsrdma.a

pal_SRC = main.cpp pal.cpp job.cpp config.cpp

pal_OBJS = $(pal_SRC:.cpp=.o)

all: pal

pal: $(DEP) $(pal_OBJS)
	$(CXX) $(pal_OBJS) $(LDFLAGS) -o $@

server: $(DEP) server.o 
	$(CXX) server.o  $(LDFLAGS) -o $@

client: $(DEP) client.o 
	$(CXX) client.o  $(LDFLAGS) -o $@

clean:
	rm -rf $(pal_OBJS) pal server client server.o client.o

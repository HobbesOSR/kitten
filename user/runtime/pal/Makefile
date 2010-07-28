
LDFLAGS =
LDFLAGS += -L$(OPENIB)/lib

LDFLAGS += -lrdmacm
LDFLAGS += -libverbs
LDFLAGS += -lpthread
LDFLAGS += -ldl

CPPFLAGS = -Wall
CPPFLAGS += -Iinclude
CPPFLAGS += -I../pct/include
CPPFLAGS += -I$(OPENIB)/include
CPPFLAGS += -I../librdmaPlus

all: pal

pal_SRC = main.cpp pal.cpp job.cpp config.cpp

pal_OBJS = $(pal_SRC:.cpp=.o)

all: pal

pal: $(pal_OBJS)
	$(CXX) $(pal_OBJS) $(LDFLAGS) -o $@

clean:
	rm -rf $(pal_OBJS) pal

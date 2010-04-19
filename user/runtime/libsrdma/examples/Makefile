
LDFLAGS = \
	-L$(HOME)/IB/install/lib\
	-L../src\
	-lpthread\
	-ldat2\
	-lsrdma

CPPFLAGS = -Wall
CPPFLAGS += -I../include -I$(HOME)/IB/install/include

DEP=../src/libsrdma.a

test_SRC = memWrite.cpp memRead.cpp sendRecv.cpp
test_OBJS = $(test_SRC:.cpp=.o)
test_TARGETS = $(test_SRC:.cpp=)

all: $(test_TARGETS)

memWrite: $(DEP) memWrite.o
	$(CXX) $@.o $(LDFLAGS) -o $@

memRead: $(DEP) memRead.o
	$(CXX) $@.o $(LDFLAGS) -o $@

sendRecv: $(DEP) sendRecv.o
	$(CXX) $@.o $(LDFLAGS) -o $@

clean:
	rm -rf $(test_OBJS) $(test_TARGETS)

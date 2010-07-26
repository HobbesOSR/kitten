
MPI=$(HOME)/hg3/kitten/user/libs/openmpi-1.2.9/install
LDFLAGS = 
LDFLAGS += -L$(HOME)/IB/install/lib 
LDFLAGS += -L../libmalloc
LDFLAGS += -Wl,-whole-archive -lmalloc -Wl,-no-whole-archive
LDFLAGS	+= -Wl,-whole-archive -lmlx4 -Wl,-no-whole-archive
LDFLAGS += -libverbs
LDFLAGS += -lrt
LDFLAGS += -lpthread
LDFLAGS += -ldl

MPI_LDFLAGS += -L../liblwkOrte
#MPI_LDFLAGS += -L ../../libs/openmpi-1.2.9/install/lib
MPI_LDFLAGS += -L $(MPI)/lib
MPI_LDFLAGS += -lmpi 
MPI_LDFLAGS += -lopen-rte 
MPI_LDFLAGS += -lopen-pal 
MPI_LDFLAGS += -llwkOrte

CPPFLAGS = 
CPPFLAGS += -I $(MPI)/include

all: hello mpi

CC=pgcc
CXX=pgcpp

hello: hello.o
	$(CXX) -Bstatic hello.o $(LDFLAGS) -o $@

mpi: mpi.o
	$(CXX) -Bstatic mpi.o $(MPI_LDFLAGS) $(LDFLAGS) -o $@

clean:
	rm -rf mpi.o hello.o mpi hello


LDFLAGS = 
LDFLAGS += -L$(OPENIB)/lib64 -L$(OPENIB)/lib
LDFLAGS += -L../libmalloc
LDFLAGS	+= -Wl,-whole-archive -lmalloc -Wl,-no-whole-archive
LDFLAGS	+= -Wl,-whole-archive -lmlx4 -Wl,-no-whole-archive
LDFLAGS += -libverbs
LDFLAGS += -lrt
LDFLAGS += -lpthread
LDFLAGS += -ldl

MPI_LDFLAGS =
MPI_LDFLAGS += -L../liblwkOrte
MPI_LDFLAGS += -L $(OPENMPI)/lib
MPI_LDFLAGS += -lmpi 
MPI_LDFLAGS += -lopen-rte 
MPI_LDFLAGS += -lopen-pal 
MPI_LDFLAGS += -llwkOrte

CPPFLAGS = -Wall -DUSING_LWK
CPPFLAGS += -I $(OPENMPI)/include

all: hello mpi

CC=gcc
CXX=g++

hello: hello.o
	$(CXX) -static hello.o $(LDFLAGS) -o $@

mpi: mpi.o
	$(CXX) -static mpi.o $(MPI_LDFLAGS) $(LDFLAGS) -o $@

clean:
	rm -rf mpi.o hello.o mpi hello

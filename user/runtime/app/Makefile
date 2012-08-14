
LDFLAGS = 
LDFLAGS += -L$(OPENIB)/lib64 -L$(OPENID)/lib
LDFLAGS += -L../libmalloc
LDFLAGS += -Wl,-whole-archive -lmalloc -Wl,-no-whole-archive
LDFLAGS	+= -Wl,-whole-archive -lmlx4 -Wl,-no-whole-archive
LDFLAGS += -libverbs
LDFLAGS += -lrt
LDFLAGS += -lpthread
LDFLAGS += -ldl

MPI_LDFLAGS += -L../liblwkOrte
MPI_LDFLAGS += -L $(OPENMPI)/lib
MPI_LDFLAGS += -lmpi 
MPI_LDFLAGS += -lopen-rte 
MPI_LDFLAGS += -lopen-pal 
MPI_LDFLAGS += -llwkOrte

CPPFLAGS = 
CPPFLAGS += -I $(OPENMPI)/include

all: hello mpi

CC=gcc
CXX=gcc

hello: hello.o
	$(CXX) -Bstatic hello.o $(LDFLAGS) -o $@

mpi: mpi.o
	$(CXX) -Bstatic mpi.o $(MPI_LDFLAGS) $(LDFLAGS) -o $@

clean:
	rm -rf mpi.o hello.o mpi hello

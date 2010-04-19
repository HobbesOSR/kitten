
CPPFLAGS = -Wall
CPPFLAGS += -I../include
CPPFALGS += -I$(HOME)/IB/install/include

libsrdma_CPP_SOURCE = dm.cpp dto.cpp dtoSend.cpp dtoMem.cpp dtoRecv.cpp

libsrdma_OBJS=$(libsrdma_CPP_SOURCE:.cpp=.o)

libsrdma.a: $(libsrdma_OBJS) 
	ar rcs $@ $(libsrdma_OBJS)

clean:
	rm -rf $(libsrdma_OBJS) libsrdma.a 

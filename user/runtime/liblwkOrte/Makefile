CPPFLAGS += -DHAVE_CONFIG_H 
CPPFLAGS += -I. 
CPPFLAGS += -I$(OPENMPI_SRC)
CPPFLAGS += -I$(OPENMPI_SRC)/build/orte/include
CPPFLAGS += -I$(OPENMPI_SRC)/build/opal/include
CPPFLAGS += -I$(OPENMPI_SRC)/orte/include
CPPFLAGS += -I$(OPENMPI_SRC)/opal/include

CC=gcc
CXX=g++

liblwkOrte_CPP_SOURCE = lwkOrteRmlBinding.cpp lwkOrteRml.cpp

liblwkOrte_OBJS=$(liblwkOrte_CPP_SOURCE:.cpp=.o)

liblwkOrte.a: $(liblwkOrte_OBJS) 
	ar rcs $@ $(liblwkOrte_OBJS)

clean:
	rm -rf $(liblwkOrte_OBJS) liblwkOrte.a 

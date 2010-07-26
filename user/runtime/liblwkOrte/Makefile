
OPENMPI=/home/mjleven/hg3/kitten/user/libs/openmpi-1.2.9/

#CPPFLAGS = -Wall
CPPFLAGS += -DHAVE_CONFIG_H 
CPPFLAGS += -I. 
CPPFLAGS += -I$(OPENMPI)/build/opal/include
CPPFLAGS += -I$(OPENMPI)/build/orte/include
CPPFLAGS += -I$(OPENMPI)/build/ompi/include
CPPFLAGS += -I$(OPENMPI)/orte/mca/rml/cnos
CPPFLAGS += -I$(OPENMPI)/opal/include
CPPFLAGS += -I$(OPENMPI)/orte/include
CPPFLAGS += -I$(OPENMPI)/ompi/include
CPPFLAGS += -I$(OPENMPI)

CC=pgcc
CXX=pgcpp

liblwkOrte_CPP_SOURCE = lwkOrteRmlBinding.cpp lwkOrteRml.cpp

liblwkOrte_OBJS=$(liblwkOrte_CPP_SOURCE:.cpp=.o)

liblwkOrte.a: $(liblwkOrte_OBJS) 
	ar rcs $@ $(liblwkOrte_OBJS)

clean:
	rm -rf $(liblwkOrte_OBJS) liblwkOrte.a 




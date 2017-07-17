STRUCTURES_PATH = ../structures
BOOST_PATH = ../boost_install
BOOST_INCLUDE_PATH = $(BOOST_PATH)/include/current
BOOST_LIB_PATH = $(BOOST_PATH)/lib
CC = $(CXX)
CXXFLAGS += -g -Wall -I$(STRUCTURES_PATH) -I$(XERCESCROOT)/include -I$(BOOST_INCLUDE_PATH)
LDFLAGS += -L$(XERCESCROOT)/lib -L$(STRUCTURES_PATH) 
LDLIBS += -lxerces-c -lTipsy

TARGETS = simload

all: $(TARGETS)

.PHONY: all clean

simload: SiXFormat.o TipsyFormat.o
simload2: SiXFormat2.o

TipsyFormat.o: Simulation.h

SiXFormat.o: Simulation.h
SiXFormat2.o: Simulation2.h

simload.o: Simulation.h SiXFormat.h TipsyFormat.h
simload2.o: Simulation2.h SiXFormat2.h

group_test: SiXFormat.o
group_test.o: Group.h

clean:
	rm -f *~ *.o core.* $(TARGETS)
	

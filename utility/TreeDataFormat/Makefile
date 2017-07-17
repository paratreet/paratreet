STRUCTURES_PATH = ../structures
CC = $(CXX)
CXXFLAGS += -Wall -g -O3 -I$(STRUCTURES_PATH)
LDFLAGS += -L$(STRUCTURES_PATH)
LDLIBS += -lTipsy

TARGETS = tipsy2tree see_header browse_field merge_tipsy_array ss2salsa \
	sub_field minmax_field div_field
#check_tree convert_to_ascii histogram_field convert_ascii_field merge_tipsy_array

all:: $(TARGETS)

tipsy2tree.o: $(STRUCTURES_PATH)/libTipsy.a
tipsy2tree.o: LDFLAGS += -lpopt
ss2salsa.o: $(STRUCTURES_PATH)/libTipsy.a
ss2salsa.o: LDFLAGS += -lpopt
browse_field.o: ../structures/tree_xdr.h

merge_tipsy_array.o: CXXFLAGS += -I../SimulationHandler -I$(XERCESCROOT)/include
merge_tipsy_array: LDFLAGS += -L$(XERCESCROOT)/lib
merge_tipsy_array: LDLIBS += -lxerces-c
merge_tipsy_array: ../SimulationHandler/SiXFormat.o merge_tipsy_array.o

clean::
	rm -f *~ *.o core.* $(TARGETS)
	

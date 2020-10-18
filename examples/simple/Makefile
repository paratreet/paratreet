CHARM_HOME ?= $(HOME)/charm-paratreet
BASE_PATH=$(shell realpath "$(shell pwd)/../..")
PARATREET_PATH = $(BASE_PATH)/src
STRUCTURE_PATH = $(BASE_PATH)/utility/structures
OPTS = -g -I$(STRUCTURE_PATH) -I$(PARATREET_PATH) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(PARATREET_PATH) -lparatreet

BINARY = Main
OBJS = Main.o

all: $(BINARY)

$(BINARY).decl.h: $(BINARY).ci
	$(CHARMC) $<

$(BINARY): $(BINARY).decl.h $(OBJS)
	$(CHARMC) -language charm++ -o $(BINARY) $(OBJS) $(LD_LIBS)

Main.o: Main.C
	$(CHARMC) -c $<

test: all
	./charmrun ./$(BINARY) -f $(BASE_PATH)/inputgen/100k.tipsy -l 1 -u 2 +p1 ++ppn 1 +pemap 1 +commap 0 ++local

clean:
	rm -f *.decl.h *.def.h conv-host *.o $(BINARY) charmrun

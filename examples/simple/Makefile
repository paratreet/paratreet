CHARM_HOME ?= $(HOME)/charm-paratreet
BASE_PATH=$(shell realpath "$(shell pwd)/../..")
PARATREET_PATH = $(BASE_PATH)/src
STRUCTURE_PATH = $(BASE_PATH)/utility/structures
OPTS = -g -O0 -I$(STRUCTURE_PATH) -I$(PARATREET_PATH) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(PARATREET_PATH) -lparatreet -lSegFault

all: Gravity SPH
VISITORS = DensityVisitor.h PressureVisitor.h GravityVisitor.h
OTHERS = CountManager.h NeighborListCollector.h

Main.decl.h: Main.ci
	$(CHARMC) $<

Gravity: Main.decl.h Gravity.o Main.o
	$(CHARMC) -language charm++ -module CommonLBs -o Gravity Gravity.o Main.o $(LD_LIBS)

SPH: Main.decl.h SPH.o Main.o
	$(CHARMC) -language charm++ -module CommonLBs -o SPH SPH.o Main.o $(LD_LIBS)

Gravity.o: Gravity.C
	$(CHARMC) -c $<

SPH.o: SPH.C
	$(CHARMC) -c $<

Main.o: Main.C $(VISITORS) $(OTHERS)
	$(CHARMC) -c $<

test: all
	./charmrun ++local ./Gravity +auto-provision -f ../../inputgen/100k.tipsy -t bin -d sfc -n 512 -p 512

clean:
	rm -f *.decl.h *.def.h conv-host *.o Gravity SPH charmrun

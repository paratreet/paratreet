CHARM_HOME ?= $(HOME)/charm-paratreet
BASE_PATH=$(shell realpath "$(shell pwd)/..")
PARATREET_PATH = $(BASE_PATH)/src
STRUCTURE_PATH = $(BASE_PATH)/utility/structures
OPTS = -g -Ofast -I$(STRUCTURE_PATH) -I$(PARATREET_PATH) -DCOUNT_INTERACTIONS=0 -DCMK_LB_USER_DATA -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(PARATREET_PATH) -lparatreet
LB_LIBS = -module CommonLBs -module DistributedPrefixLB -module PrefixLB -module AverageSmoothLB
PROJ_LIBS = -tracemode projections

all: Gravity SPH Collision
VISITORS = DensityVisitor.h PressureVisitor.h GravityVisitor.h CollisionVisitor.h
OTHERS = CountManager.h

Main.decl.h: Main.ci
	$(CHARMC) $<

Gravity: Main.decl.h Main.o Gravity.o
	$(CHARMC) -language charm++ $(LB_LIBS) -o Gravity Gravity.o Main.o $(LD_LIBS)
	$(CHARMC) -language charm++ $(LB_LIBS) $(PROJ_LIBS) -o Gravity_proj Gravity.o Main.o $(LD_LIBS)

Collision: Main.decl.h Main.o Collision.o
	$(CHARMC) -language charm++ $(LB_LIBS) -o Collision Collision.o Main.o $(LD_LIBS)
	$(CHARMC) -language charm++ $(LB_LIBS) $(PROJ_LIBS) -o Collision_proj Gravity.o Main.o $(LD_LIBS)

SPH: Main.decl.h Main.o SPH.o
	$(CHARMC) -language charm++ $(LB_LIBS) -o SPH SPH.o Main.o $(LD_LIBS)
	$(CHARMC) -language charm++ $(LB_LIBS) $(PROJ_LIBS) -o SPH_proj Gravity.o Main.o $(LD_LIBS)

Gravity.o: Gravity.C Main.decl.h
	$(CHARMC) -c $<

Collision.o: Collision.C Main.decl.h
	$(CHARMC) -c $<

SPH.o: SPH.C SPHUtils.h NeighborListCollector.h Main.decl.h
	$(CHARMC) -c $<

Main.o: Main.C $(VISITORS) $(OTHERS) Main.decl.h
	$(CHARMC) -c $<

test: all
	./charmrun ./Gravity -f $(BASE_PATH)/inputgen/100k.tipsy -d sfc +p3 ++ppn 3 +pemap 1-3 +commap 0 ++local

clean:
	rm -f *.decl.h *.def.h conv-host *.o Gravity SPH Collision charmrun

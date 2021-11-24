include ../src/Makefile.common

LBS = CommonLBs PrefixLB OrbLB #AverageSmoothLB DiffusionLB DistributedPrefixLB DistributedOrbLB
LB_LIBS = $(foreach m, $(LBS), -module $(m))
OPTS = -g -O3 $(INCLUDES) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)

all: Gravity SPH Collision VisitAll
VISITORS = DensityVisitor.h PressureVisitor.h GravityVisitor.h CollisionVisitor.h NeighborListCollector.h VisitAllVisitor.h
OTHERS = CountManager.h

debug:
	echo $(LB_LIBS)

Main.decl.h: Main.ci
	$(CHARMC) $<

Gravity: Main.decl.h Main.o Gravity.o moments.o ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o Gravity Gravity.o moments.o Main.o $(LD_LIBS)

Collision: Main.decl.h Main.o Collision.o moments.o ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o Collision Collision.o Main.o moments.o $(LD_LIBS)

SPH: Main.decl.h Main.o SPH.o moments.o ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o SPH SPH.o Main.o moments.o $(LD_LIBS)

VisitAll: Main.decl.h Main.o VisitAll.o moments.o ../src/libparatreet.a
	$(CHARMC) -language charm++ -module CommonLBs -o VisitAll VisitAll.o Main.o moments.o $(LD_LIBS)

Gravity.o: Gravity.C Main.decl.h
	$(CHARMC) -c $<

Collision.o: Collision.C Main.decl.h
	$(CHARMC) -c $<

SPH.o: SPH.C SPHUtils.h Main.decl.h
	$(CHARMC) -c $<

VisitAll.o: VisitAll.C Main.decl.h
	$(CHARMC) -c $<

Main.o: Main.C CentroidData.h moments.h MultipoleMoments.h $(VISITORS) $(OTHERS) Main.decl.h
	$(CHARMC) -c $<

moments.o: moments.C
	$(CHARMC) -c $<

test: all
	./charmrun ./Gravity -f $(BASE_PATH)/inputgen/100k.tipsy -d sfc +p3 ++ppn 3 +pemap 1-3 +commap 0 ++local

clean:
	rm -f *.decl.h *.def.h conv-host *.o Gravity SPH Collision charmrun

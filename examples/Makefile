include ../src/Makefile.common

# comment out -DFOF if not using FoF to not waste runtime
LBS = CommonLBs PrefixLB OrbLB #AverageSmoothLB DiffusionLB DistributedPrefixLB DistributedOrbLB
LB_LIBS = $(foreach m, $(LBS), -module $(m))
OPTS = -g -O3 $(INCLUDES) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 -DHEXADECAPOLE $(MAKE_OPTS) -DFOF -DANCHOR_ALGO
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)

all: Gravity SPH Collision FoFApp
DATA = CentroidData.h MultipoleMoments.h
VISITORS = DensityVisitor.h PressureVisitor.h GravityVisitor.h CollisionVisitor.h

debug:
	echo $(LB_LIBS)

Main.decl.h: Main.ci $(DATA)
	$(CHARMC) $<

Gravity: Main.decl.h Main.o Gravity.o Ewald.o moments.o ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o Gravity Gravity.o Ewald.o moments.o Main.o $(LD_LIBS)

Collision: Main.decl.h Main.o Collision.o moments.o Ewald.o ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o Collision Collision.o Main.o moments.o Ewald.o $(LD_LIBS)

SPH: Main.decl.h Main.o SPH.o moments.o Ewald.o ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o SPH SPH.o Main.o moments.o Ewald.o $(LD_LIBS)

Gravity.o: Gravity.C Main.decl.h
	$(CHARMC) -c $<

Ewald.o: Ewald.C Main.decl.h
	$(CHARMC) -c $<

Collision.o: Collision.C Main.decl.h
	$(CHARMC) -c $<

SPH.o: SPH.C SPHUtils.h Main.decl.h
	$(CHARMC) -c $<

Main.o: Main.C $(VISITORS) Main.decl.h
	$(CHARMC) -c $<

moments.o: moments.C
	$(CHARMC) -c $<

FoF.decl.h: ./FoF/FoF.ci $(DATA)
	$(CHARMC) $<

FoFApp: FoF.decl.h FoF.o ../src/libparatreet.a moments.o
	$(CHARMC) -DFOF -language charm++ $(LB_LIBS) -o FoFApp FoF.o moments.o $(LD_LIBS)

FoF.o: ./FoF/FoF.C ./FoF.decl.h ./FoF/FoFVisitor.h
	$(CHARMC) -DFOF -c $<

test: all
	./charmrun ./Gravity -f $(BASE_PATH)/inputgen/100k.tipsy -d sfc +p3 ++ppn 3 +pemap 1-3 +commap 0 ++local

clean:
	rm -f *.decl*.h *.def*.h conv-host *.o ./FoF/*.decl*.h ./FoF/*.def*.h Gravity SPH Collision FoFApp* charmrun* moduleinit*

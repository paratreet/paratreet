include ../src/Makefile.common

LBS = CommonLBs PrefixLB OrbLB #AverageSmoothLB DiffusionLB DistributedPrefixLB DistributedOrbLB
LB_LIBS = $(foreach m, $(LBS), -module $(m))
OPTS = -g -O3 $(INCLUDES) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 -DHEXADECAPOLE $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)

all: Gravity SPH Collision
DATA = Particle.h MultipoleMoments.h BoundingBox.h CentroidData.h
VISITORS = DensityVisitor.h PressureVisitor.h GravityVisitor.h CollisionVisitor.h
OBJS = Particle.o BoundingBox.o CentroidData.o Ewald.o moments.o Main.o

debug:
	echo $(LB_LIBS)

Main.decl.h: Main.ci $(DATA) ../src/libparatreet.a # I could instead list all the headers
	$(CHARMC) $<

Gravity: Main.decl.h Main.o Gravity.o $(OBJS) ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o Gravity Gravity.o $(OBJS) $(LD_LIBS)

Collision: Main.decl.h Main.o Collision.o $(OBJS) ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o Collision Collision.o $(OBJS) $(LD_LIBS)

SPH: Main.decl.h Main.o SPH.o $(OBJS) ../src/libparatreet.a
	$(CHARMC) -language charm++ $(LB_LIBS) -o SPH SPH.o $(OBJS) $(LD_LIBS)

Particle.o: Particle.C Particle.h
	$(CHARMC) -c $<

BoundingBox.o: BoundingBox.C Particle.h
	$(CHARMC) -c $<

CentroidData.o: CentroidData.C CentroidData.h Particle.h BoundingBox.h moments.h
	$(CHARMC) -c $<

Gravity.o: Gravity.C Main.decl.h
	$(CHARMC) -c $<

Ewald.o: Ewald.C Ewald.h EwaldData.h
	$(CHARMC) -c $<

Collision.o: Collision.C Main.decl.h
	$(CHARMC) -c $<

SPH.o: SPH.C SPHUtils.h Main.decl.h
	$(CHARMC) -c $<

Main.o: Main.C $(VISITORS) Main.decl.h
	$(CHARMC) -c $<

moments.o: moments.C moments.h
	$(CHARMC) -c $<

test: all
	./charmrun ./Gravity -f $(BASE_PATH)/inputgen/100k.tipsy -d sfc +p3 ++ppn 3 +pemap 1-3 +commap 0 ++local

clean:
	rm -f *.decl.h *.def.h conv-host *.o Gravity SPH Collision charmrun moduleinit*

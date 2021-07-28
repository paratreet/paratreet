CHARM_HOME ?= $(HOME)/charm-paratreet
STRUCTURE_PATH = ../utility/structures
OPTS = -g -Ofast -I$(STRUCTURE_PATH) -DCOUNT_INTERACTIONS=0 -DCMK_LB_USER_DATA -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)

OBJS = Paratreet.o Reader.o Writer.o Particle.o BoundingBox.o Decomposition.o Modularization.o TreeSpec.o
TIPSY_OBJS = NChilReader.o SS.o TipsyFile.o TipsyReader.o hilbert.o

UTILITY_HEADERS = common.h Utility.h $(STRUCTURE_PATH)/Vector3D.h $(STRUCTURE_PATH)/SFC.h
CORE_HEADERS = BoundingBox.h BufferedVec.h CentroidData.h MultiData.h Node.h NodeWrapper.h ParticleComp.h ParticleMsg.h Splitter.h
IMPL_HEADERS = CacheManager.h Configuration.h Driver.h Partition.h Reader.h Resumer.h Splitter.h Subtree.h Traverser.h TreeCanopy.h LBCommon.h

LBS = DistributedOrbLB
LB_LIBS = $(foreach m, $(LBS), libmodule$(m).a)

all: lib

lib: $(OBJS) $(LB_LIBS)
	ar x $(STRUCTURE_PATH)/libTipsy.a
	ar cr libparatreet.a $(OBJS) $(TIPSY_OBJS)
	rm -f $(TIPSY_OBJS)
	ranlib libparatreet.a

paratreet.decl.h: paratreet.ci
	$(CHARMC) $<

Paratreet.o: $(UTILITY_HEADERS) $(CORE_HEADERS) $(IMPL_HEADERS) paratreet.decl.h
	$(CHARMC) -c Paratreet.C

%.o: %.C %.h paratreet.decl.h
	$(CHARMC) -c $<

libmodule%.a : %.ci %.C %.h LBCommon.h
	$(CHARMC) $<
	$(CHARMC) -o $@ $(word 2,$^)

clean:
	rm -f *.decl.h *.def.h conv-host *.o libmodule*.a libparatreet.a charmrun

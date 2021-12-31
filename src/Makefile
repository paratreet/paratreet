include Makefile.common

OPTS = -g -Ofast $(INCLUDES) -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)

OBJS = Paratreet.o Loadable.o Reader.o Writer.o Particle.o BoundingBox.o Decomposition.o Modularization.o TreeSpec.o ThreadStateHolder.o
TIPSY_OBJS = NChilReader.o SS.o TipsyFile.o TipsyReader.o hilbert.o

UTILITY_HEADERS = common.h Utility.h $(STRUCTURE_PATH)/Vector3D.h $(STRUCTURE_PATH)/SFC.h
CORE_HEADERS = BoundingBox.h BufferedVec.h MultiData.h Node.h NodeWrapper.h ParticleMsg.h Splitter.h
IMPL_HEADERS = CacheManager.h Configuration.h Driver.h Partition.h Reader.h Resumer.h Splitter.h Subtree.h Traverser.h TreeCanopy.h

LBS = PrefixLB OrbLB #AverageSmoothLB DiffusionLB DistributedPrefixLB DistributedOrbLB
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

libmodule%.a : loadbalancers/%.ci loadbalancers/%.C loadbalancers/%.h LBCommon.h
	$(CHARMC) $<
	$(CHARMC) -o $@ $(word 2,$^)

clean:
	rm -f *.decl.h *.def.h conv-host *.o *.a

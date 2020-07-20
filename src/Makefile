CHARM_HOME ?= $(HOME)/charm-paratreet
STRUCTURE_PATH = ../utility/structures
OPTS = -g -I$(STRUCTURE_PATH) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(STRUCTURE_PATH) -lTipsy

BINARY = paratreet
OBJS = Main.o Reader.o Particle.o BoundingBox.o Decomposition.o TreeSpec.o

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CHARMC) -language charm++ -o $(BINARY) $(OBJS) $(LD_LIBS)

proj: $(OBJS)
	$(CHARMC) -language charm++ -tracemode projections -o $(BINARY) $(OBJS) $(LD_LIBS)

$(BINARY).decl.h: $(BINARY).ci
	$(CHARMC) $<

common.h: $(STRUCTURE_PATH)/Vector3D.h $(STRUCTURE_PATH)/SFC.h Utility.h

Main.o: Main.C $(BINARY).decl.h common.h Reader.h TreeSpec.h BoundingBox.h BufferedVec.h TreeCanopy.h CacheManager.h Node.h Resumer.h Traverser.h Driver.h GravityVisitor.h DensityVisitor.h PressureVisitor.h CountVisitor.h Writer.h Partition.h Subtree.h NodeWrapper.h
	$(CHARMC) -c $<

CacheManager.h: $(BINARY).decl.h

%.o: %.C %.h $(BINARY).decl.h
	$(CHARMC) -c $<

test: all
	./charmrun ./paratreet -f ../inputgen/100k.tipsy +p3 ++ppn 3 +pemap 1-3 +commap 0 ++local

clean:
	rm -f *.decl.h *.def.h conv-host *.o $(BINARY) charmrun

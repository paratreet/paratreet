CHARM_HOME ?= ~/charm
STRUCTURE_PATH = ../utility/structures
OPTS = -g -I$(STRUCTURE_PATH) -DDEBUG
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(STRUCTURE_PATH) -lTipsy

BINARY = simple
OBJS = Main.o Reader.o Particle.o BoundingBox.o Decomposer.o TreePiece.o TreeElements.o

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CHARMC) -language charm++ -o $(BINARY) $(OBJS) $(LD_LIBS)

proj: $(OBJS)
	$(CHARMC) -language charm++ -tracemode projections -o $(BINARY) $(OBJS) $(LD_LIBS)

$(BINARY).decl.h: $(BINARY).ci
	$(CHARMC)  $(BINARY).ci

clean:
	rm -f *.decl.h *.def.h conv-host *.o $(BINARY) charmrun

common.h: $(STRUCTURE_PATH)/Vector3D.h $(STRUCTURE_PATH)/SFC.h

Main.o: Main.C $(BINARY).decl.h common.h Reader.h Decomposer.h TreePiece.h BoundingBox.h BufferedVec.h
	$(CHARMC) -c $<

Reader.h: $(BINARY).decl.h common.h Particle.h BoundingBox.h Splitter.h

Reader.o: Reader.C Reader.h
	$(CHARMC) -c $<

Particle.h: $(BINARY).decl.h common.h

Particle.o: Particle.C Particle.h
	$(CHARMC) -c $<

BoundingBox.h: $(STRUCTURE_PATH)/OrientedBox.h common.h

BoundingBox.o: BoundingBox.C BoundingBox.h
	$(CHARMC) -c $<

Decomposer.h: $(BINARY).decl.h common.h Splitter.h TreeElements.h

Decomposer.o: Decomposer.C Decomposer.h BufferedVec.h Utility.h
	$(CHARMC) -c $<

TreePiece.h: $(BINARY).decl.h common.h Particle.h Node.h Utility.h

TreePiece.o: TreePiece.C TreePiece.h Reader.h TreeElements.h
	$(CHARMC) -c $<

TreeElements.h: $(BINARY).decl.h common.h Utility.h

TreeElements.o: TreeElements.C TreeElements.h
	$(CHARMC) -c $<

test: all
	$(call run, ./$(BINARY) +p4 4)

CHARM_HOME ?= ~/charm
STRUCTURE_PATH = ../utility/structures
OPTS = -g -I$(STRUCTURE_PATH) -DDEBUG
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(STRUCTURE_PATH) -lTipsy

BINARY = simple
OBJS = Main.o Reader.o Particle.o BoundingBox.o

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

Main.o: Main.C $(BINARY).decl.h common.h Reader.h TreePiece.h BoundingBox.h BufferedVec.h TreeElement.h CentroidVisitor.h GravityVisitor.h Utility.h
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

TreePiece.h: $(BINARY).decl.h $(BINARY).def.h common.h Particle.h Node.h Utility.h Reader.C

TreeElement.h: $(BINARY).decl.h $(BINARY).def.h CentroidVisitor.h

CentroidVisitor.h: $(BINARY).decl.h common.h CentroidData.h

CentroidData.h: common.h

GravityVisitor.h: $(BINARY).decl.h CentroidData.h

test: all
	./charmrun ++local ./simple -f ../inputgen/1k.tipsy +p4

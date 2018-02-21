CHARM_HOME ?= ~/charm
STRUCTURE_PATH = ../utility/structures
OPTS = -g -I$(STRUCTURE_PATH) -DDEBUG
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(STRUCTURE_PATH) -lTipsy

BINARY = simple
OBJS = Main.o Reader.o Particle.o BoundingBox.o TreePiece.o CentroidCalculator.o CentroidVisitor.o

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

Main.o: Main.C $(BINARY).decl.h common.h Reader.h TreePiece.h BoundingBox.h BufferedVec.h CentroidCalculator.h CentroidVisitor.h
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

TreePiece.h: $(BINARY).decl.h common.h Particle.h Node.h Utility.h CentroidVisitor.h

TreePiece.o: TreePiece.C TreePiece.h Reader.h
	$(CHARMC) -c $<

CentroidCalculator.h: $(BINARY).decl.h CentroidVisitor.h CentroidData.h

CentroidCalculator.o: CentroidCalculator.C CentroidCalculator.h
	$(CHARMC) -c $<

CentroidVisitor.h: common.h CentroidData.h

CentroidVisitor.o: CentroidVisitor.C CentroidVisitor.h CentroidCalculator.h
	$(CHARMC) -c $<

CentroidData.h: common.h

test: all
	./charmrun ++local ./simple -f ../inputgen/10k.tipsy +p4

CHARM_HOME ?= ~/UIUC/charm
OPTS = -O3 -g -I../utility/structures -DDEBUG
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L../utility/structures -lTipsy

BINARY = simple
OBJS = Main.o Reader.o Particle.o BoundingBox.o Decomposer.o TreePiece.o Node.o

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CHARMC) -language charm++ -o $(BINARY) $(OBJS) $(LD_LIBS)

proj: $(OBJS)
	$(CHARMC) -language charm++ -tracemode projections -o $(BINARY) $(OBJS) $(LD_LIBS)

$(BINARY).decl.h: $(BINARY).ci
	$(CHARMC)  $(BINARY).ci

clean:
	rm -f *.decl.h *.def.h conv-host *.o $(BINARY) charmrun

Main.o: Main.C $(BINARY).decl.h common.h Reader.h BoundingBox.h BufferedVec.h TreePiece.h
	$(CHARMC) -c $<

Reader.o: Reader.C Reader.h Utility.h
	$(CHARMC) -c $<

Particle.o: Particle.C Particle.h
	$(CHARMC) -c $<

BoundingBox.o: BoundingBox.C BoundingBox.h
	$(CHARMC) -c $<

Decomposer.o: Decomposer.C Decomposer.h
	$(CHARMC) -c $<

TreePiece.o: TreePiece.C TreePiece.h
	$(CHARMC) -c $<

Node.o: Node.C Node.h
	$(CHARMC) -c $<

test: all
	$(call run, ./$(BINARY) +p4 4)

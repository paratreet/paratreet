CHARM_HOME = ~/work/charm
OPTS = -O3 -I../utility/structures -DDEBUG
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L../utility/structures -lTipsy

BINARY = simple
OBJS = Main.o Reader.o Particle.o BoundingBox.o Splitter.o

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CHARMC) -language charm++ -o $(BINARY) $(OBJS) $(LD_LIBS)

proj: $(OBJS)
	$(CHARMC) -language charm++ -tracemode projections -o $(BINARY) $(OBJS) $(LD_LIBS)

$(BINARY).decl.h: $(BINARY).ci
	$(CHARMC)  $(BINARY).ci

clean:
	rm -f *.decl.h *.def.h conv-host *.o $(BINARY) charmrun

Main.o: Main.C $(BINARY).decl.h common.h Reader.h BoundingBox.h BufferedVec.h Splitter.h
	$(CHARMC) -c $<

Reader.o: Reader.C Reader.h Utility.h
	$(CHARMC) -c $<

Particle.o: Particle.C Particle.h
	$(CHARMC) -c $<

BoundingBox.o: BoundingBox.C BoundingBox.h
	$(CHARMC) -c $<

Splitter.o: Splitter.C Splitter.h Utility.h
	$(CHARMC) -c $<

test: all
	$(call run, ./$(BINARY) +p4 4)

CHARM_HOME ?= ~/charm
STRUCTURE_PATH = ../utility/structures
OPTS = -g -I$(STRUCTURE_PATH) -DDEBUG
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(STRUCTURE_PATH) -lTipsy

BINARY = simple
OBJS = Main.o Particle.o

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

Main.o: Main.C $(BINARY).decl.h common.h Node.h Test.h
	$(CHARMC) -c $<

Node.h: common.h Particle.h

Test.h: simple.decl.h common.h Node.h

Particle.h: $(BINARY).decl.h common.h

Particle.o: Particle.C Particle.h
	$(CHARMC) -c $<

test: all
	./charmrun ++local ./simple +p4

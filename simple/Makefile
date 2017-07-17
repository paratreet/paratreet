CHARM_HOME = ~/work/charm
OPTS = -O3 -I../utility/structures
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L../utility/structures -lTipsy

BINARY = simple
OBJS = $(BINARY).o

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CHARMC) -language charm++ -o $(BINARY) $(OBJS) $(LD_LIBS)

proj: $(OBJS)
	$(CHARMC) -language charm++ -tracemode projections -o $(BINARY) $(OBJS) $(LD_LIBS)

$(BINARY).decl.h: $(BINARY).ci
	$(CHARMC)  $(BINARY).ci

clean:
	rm -f *.decl.h *.def.h conv-host *.o $(BINARY) charmrun

$(BINARY).o: $(BINARY).C $(BINARY).decl.h
	$(CHARMC) -c $(BINARY).C

test: all
	$(call run, ./$(BINARY) +p4 4)

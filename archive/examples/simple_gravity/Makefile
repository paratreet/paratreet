CHARM_HOME = ~/tools/charm/netlrts-darwin-x86_64
OPTS = -Og -I../../include -L../../ -lparatreet
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS) -DDEBUG

OBJS = simple_gravity.o

.PHONY: all clean test

all: simple_gravity

simple_gravity: $(OBJS)
	$(CHARMC) -o $@ $^ -L../../lib/cosmo_utility/structures -lTipsy

clean:
	rm -f *.decl.h *.def.h *.o simple_gravity charmrun

test: all
	./charmrun ++local +p4 ./simple_gravity


# objects
simple_gravity.o: simple_gravity.cpp simple_gravity.decl.h simple_gravity.h
	$(CHARMC) -o $@ $<


# charm interfaces
simple_gravity.decl.h: simple_gravity.ci simple_gravity.h
	$(CHARMC) $<


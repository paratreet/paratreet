CHARM_HOME = ~/tools/charm/netlrts-darwin-x86_64
OPTS = -Og
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS) -DDEBUG

# Assume each ci file contains one module of the same name
OBJS = sfc.o reducers.o

.PHONY: all clean

all: libparatreet.a

libparatreet.a: $(OBJS)
	ar rcs $@ $^

clean:
	rm -f *.decl.h *.def.h *.o libparatreet.a


# dependencies
sfc.o: impl/sfc/sfc.cpp sfc.decl.h
reducers.o: impl/shared/reducers.cpp impl/shared/reducers.h


# objects
sfc.o:
	$(CHARMC) -o $@ $<

reducers.o:
	$(CHARMC) -o $@ $<


# charm interfaces
sfc.decl.h: impl/sfc/sfc.ci
	$(CHARMC) $<


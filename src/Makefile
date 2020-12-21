CHARM_HOME ?= $(HOME)/charm-paratreet
STRUCTURE_PATH = ../utility/structures
OPTS = -g -I$(STRUCTURE_PATH) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)

OBJS = Paratreet.o Reader.o Writer.o Particle.o BoundingBox.o Decomposition.o Modularization.o TreeSpec.o
TIPSY_OBJS = NChilReader.o SS.o TipsyFile.o TipsyReader.o hilbert.o

all: lib

lib: $(OBJS)
	ar x $(STRUCTURE_PATH)/libTipsy.a
	ar cr libparatreet.a $(OBJS) $(TIPSY_OBJS)
	rm -f $(TIPSY_OBJS)
	ranlib libparatreet.a

paratreet.decl.h: paratreet.ci
	$(CHARMC) $<

common.h: $(STRUCTURE_PATH)/Vector3D.h $(STRUCTURE_PATH)/SFC.h Utility.h

CacheManager.h: paratreet.decl.h

%.o: %.C %.h paratreet.decl.h
	$(CHARMC) -module CommonLBs -c $<

clean:
	rm -f *.decl.h *.def.h conv-host *.o libparatreet.a charmrun

include ../src/Makefile.common

LBS = CommonLBs PrefixLB OrbLB #AverageSmoothLB DiffusionLB DistributedPrefixLB DistributedOrbLB
LB_LIBS = $(foreach m, $(LBS), -module $(m))
OPTS = -g -O3 $(INCLUDES) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)

all: Main

debug:
	echo $(LB_LIBS)

Main.decl.h: Main.ci
	$(CHARMC) $<

Main: Main.decl.h Main.o ../src/libparatreet.a
	$(CHARMC) -language charm++ -module CommonLBs -o Main Main.o $(LD_LIBS)

Main.o: Main.C Main.decl.h
	$(CHARMC) -c $<

test: all
	./charmrun ./Main -f $(BASE_PATH)/inputgen/100k.tipsy -d sfc +p3 ++ppn 3

clean:
	rm -f *.decl.h *.def.h conv-host *.o Main charmrun

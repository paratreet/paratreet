CHARM_HOME ?= $(HOME)/charm-paratreet
PARATREET_PATH = /home/szaday2/workspace/paratreet/src
STRUCTURE_PATH = /home/szaday2/workspace/paratreet/utility/structures
OPTS = -g -I$(STRUCTURE_PATH) -I$(PARATREET_PATH) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(STRUCTURE_PATH) -lTipsy $(PARATREET_PATH)/*.o

BINARY = Main
OBJS = Main.o 

all: $(BINARY)

$(BINARY).decl.h: $(BINARY).ci
	$(CHARMC) $<

$(BINARY): $(BINARY).decl.h $(OBJS) 
	$(CHARMC) -language charm++ -o $(BINARY) $(OBJS) $(LD_LIBS) 

Main.o: Main.C
	$(CHARMC) -c $< 

test: all
	./charmrun ./$(BINARY) -f ../../inputgen/100k.tipsy +p3 ++ppn 3 +pemap 1-3 +commap 0 ++local

clean:
	rm -f *.decl.h *.def.h conv-host *.o $(BINARY) charmrun

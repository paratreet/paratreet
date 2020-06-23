#
#	Makefile for array and vector tools
#
CFLAGS = -O2 -g
LDLIBS = -lm
EXEDIR = .

EXE = absarr addarr divarr magvec maxarr meanarr minarr \
	rmsarr sortvec subarr gtarr sumarr vecpackunpk scalarr

all:$(EXE)

$(EXE):
	$(CC) $(CFLAGS) -o $(EXEDIR)/$@ $@.c $(LDLIBS)

clean:
	rm -f *.o $(EXE)

magvec: magvec.c
subarr: subarr.c
sortvec: sortvec.c
absarr: absarr.c
vecpackunpk: vecpackunpk.c

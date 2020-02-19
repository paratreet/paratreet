## Compilation

Dependent git repositories (currently only `utility`) are tracked as git submodules.
All the following commands are executed from the top level directory.

1. Intialize and update all submodule folders.
```
$ git submodule init
$ git submodule update
```

2. Build the Tipsy library, used for input file format.
```
$ cd utility/structures
$ ./configure
$ make
```

3. Build the ParaTreeT library.
```
$ cd src
$ make
```

## Execution

To run a 1000 particle simulation locally with 4 PEs (in SMP mode):
```
$ ./charmrun +p4 ++ppn 4 +pemap 1-3 +commap 0 ++local ./paratreet -f ../inputgen/1k.tipsy
```

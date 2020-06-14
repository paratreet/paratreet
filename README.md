# ParaTreeT: Parallel Tree Toolkit

![SMP](https://github.com/paratreet/paratreet/workflows/SMP/badge.svg?branch=master)
![Non-SMP](https://github.com/paratreet/paratreet/workflows/Non-SMP/badge.svg?branch=master)

## Installation

### 1. Build Charm++

See https://github.com/UIUC-PPL/charm for instructions on building Charm++.
We recommend building a stable release, by checking out a branch such as `v6.10.1`.

### 2. Build ParaTreeT

Dependent git repositories (currently only `utility`) are tracked as git submodules.
All the following commands are executed from the top level directory.

1. Initialize and update all submodule folders.
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
$ make -j
```

## Execution

To run a miniscule test simulation with 1000 particles split into TreePieces of at most 100 particles each, on 4 PEs (SMP mode, 1 comm thread):
```
$ ./charmrun +p3 ./paratreet -f ../inputgen/1k.tipsy -p 100 ++ppn 3 +pemap 1-3 +commap 0 ++local
```

On `jsrun` machines (e.g. LLNL Lassen, OLCF Summit) with no comm thread:
```
$ jsrun -n1 -a1 -c4 ./paratreet -f ../inputgen/1k.tipsy -p 100 +ppn 4 +pemap L0-12:4
```

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

3. Build example applications.
```
$ cd ../examples
$ make test
```

## Examples

Under the `examples/` directory, one can find example ParaTreeT applications, like:

- _Gravity_ &mdash; Implements an n-body simulation of gravitational dynamics.
- _SPH_ &mdash; Simulates hydrodynamics using the Smooth Particle Hydrodynamics (SPH) technique.
- _Collision_ &mdash; Simulates collisions between particles.

## Execution

Assuming one has an SMP-enabled build of Charm++, one can run a miniscule gravity simulation that splits 1000 particles into partitions of at most 100 particles on four PEs using:
```
$ ./charmrun +p3 ./Gravity -f ../inputgen/1k.tipsy -p 100 ++ppn 3 +pemap 1-3 +commap 0 ++local
```

On `jsrun` machines (e.g. LLNL Lassen, OLCF Summit) without a dedicated communications thread:
```
$ jsrun -n1 -a1 -c4 ./Gravity -f ../inputgen/1k.tipsy -p 100 +ppn 4 +pemap L0-12:4
```

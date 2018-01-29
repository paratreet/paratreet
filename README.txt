1. Compilation
--------------

* Dependent git repositories are tracked as git submodules.
  Changes for the ParaTreeT project are made in branch 'paratreet'.

1) Intialize and update all submodule folders (utility, psortingV).
    From top directory (paratreet):
    $ git submodule init
    $ git submodule update

2) In utility/structures, check the branch if it is 'paratreet' and build the
   Tipsy library.
    $ ./configure
    $ make

3) Build the 'simple' application.
    $ cd simple
    $ make



2. Execution
------------

- To run a 1000 particle simulation locally with 4 PEs:
    $ ./charmrun ++local ./simple +p4 -f ../inputgen/1k.tipsy

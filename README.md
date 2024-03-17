Painless: a Framework for Parallel SAT Solving 
==============================================

* Mazigh SAOUDI (mazigh.saoudi@epita.fr)
* Souheib BAARIR (souheib.baarir@lip6.fr)


Content
-------
* painless-src/:
   Contains the code of the framework.
   * clauses/:
      Contains the code to manage shared clauses.
   * working/:
      Code links to the worker organization.
   * sharing/:
      Code links to the learnt clause sharing management.
   * solvers/:
      Contains wrapper for the sequential solvers.
   * utils/:
      Contains code for clauses management. But also useful data structures.

* mapleCOMSPS/:
   Contains the code of MapleCOMSPS from the SAT Competition 17 with some little changes.

* kissat_mab/:
   Contains the code of kissat_mab.

To compile the project
----------------------
* Must install an MPI implementation, painless was tested with [OpenMpi](https://www.open-mpi.org/). 

* In the main directory use 'make' to compile the code.

* In the main directory use 'make clean' to clean.


To run painless
---------------
- All options have a default value except for the filename.
```sh
nb_cpu=6 # the number of sequential solvers to instantiate
nb_nodes=3 # the number of mpi processes to launch
timeout=1000 # the timeout before giving up (in seconds)
strat=1 # 1 2 3 4 for strategies with Hordesat, 5 for Simple. 0 is the default. It is to randomize the strategy pick, can be useful with -dist.
verbose=0 # for the logs (1 for general logs) (2 for detailed logs)
gstrat=1 # 1 for AllGatherSharing, 2 for MallobSharing, and 3 for RingSharing
filename=./inputs_example/f/f2000.cnf #path to a cnf file
```
- Modes:
``` sh
-str "one thread in charge of strengthening learnt clauses per sharing group"

-dup "remove/promote duplicate clauses, not available in StrengtheningSharing"

-dist "enable distributed mode"

-one-sharer "use one sharer for all sharing strategies"

-simp "enable some preprocessings before launching the solvers"
```
* painless with kissat:
```sh
./painless -v=$verbose -c=$nb_cpu -solver="k" -t=$timeout -shr-strat=$strat $filename
```
* painless with maple:
```sh
./painless -v=$verbose -c=$nb_cpu -t=$timeout -shr-strat=$strat $filename
```
* painless-kissat with mpi each node in a separate terminal:
```sh
mpirun -n $nb_nodes xterm -hold -e ./painless -c=$nb_cpu  -solver="k" -t=$timeout -v=$verbose -shr-strat=$strat -gshr-start=$gstrat -dist $filename 
```
* The script `scripts/launch.sh` can be used to launch a certain instance described by `parameters.sh` on multiple forumale:
```sh
./scripts/launch.sh ./file_with_paths_to_instances
```
* The file `./file_with_paths_to_instances` can be generated using the `ls` command, for example:
```sh
ls $PWD/inputs_example/f/* > instances_f.txt
```


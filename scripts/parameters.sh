# machine info
nb_physical_cores=3 #$(lscpu | grep ^Core\(s\)\\sper\\ssocket:\\s | awk '{print $4}')
# nb_physical_cores=$(lscpu | grep ^Core\(s\): | awk '{print $2}')
# used by both
nb_solvers=$nb_physical_cores #$((nb_physical_cores - 1)) # the remaning one for sharer

# painless only
verbose=0
timeout=50
nb_bloom_gstrat=1
solver="k"
lstrat=1 #1: Hordesat, 5: Simple
gstrat=2 #1: AllGather, 2:Mallob, 3:Ring
gshr_lit=$((nb_solvers * 500))
shr_sleep=500000
flags="-one-sharer" # -dup -one-sharer -dist -simp

# mpi only
#nb_procs_per_node=$(lscpu | grep ^Socket\(s\):\\s | awk '{print $2}')
nb_procs_per_node=3

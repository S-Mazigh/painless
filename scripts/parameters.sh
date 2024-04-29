# machine info
#nb_physical_cores=$(lscpu | grep ^Core\(s\)\\sper\\ssocket:\\s | awk '{print $4}')
# nb_physical_cores=$(lscpu | grep ^Core\(s\): | awk '{print $2}')
nb_physical_cores=32
# used by both
nb_solvers=$((nb_physical_cores - 1)) # the remaning one for sharer

# painless only
verbose=1
timeout=5000
solver="k"
lstrat=4
gstrat=-1
shr_sleep=100000
flags="-sbva-count=12 -ls-after-sbva=2" # -dup -one-sharer -dist -simp -no-sbva-shuffle
sbva_timeout=1000
# sbva_ls_timeout=30

# mpi only
nb_procs_per_node=$(lscpu | grep ^Socket\(s\):\\s | awk '{print $2}')
# nb_procs_per_node=3
hostfile=$HOSTFILE
nb_nodes=$(ls $hostfile | wc)
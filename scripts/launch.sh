#! /usr/bin/bash

script_dir=$(dirname $0)

echo $script_dir

source $script_dir/myLibrary.sh

# themes
colored=1
error_theme="\e[1;101;97m"
info_theme="\e[1;44m"
reset_theme="\e[0m"
log_theme="\033[1;34m"

painless_home=$(pwd)

if [ $# -lt 1 ]; then
    echo "Please enter the file with path to the formulas"
    exit 1
fi

input_files=$1

if [ ! -f $input_files ]; then
    echo "$input_files does not exist or is not a file"
    exit 1
fi

# info variables
nb_clauses=0
hostfile="$script_dir/hostfile"

if [ ! -f $hostfile ]; then
    # default_hostfile="localhost slots=$nb_physical_cores"
    echo -e "$error_theme ./hostfile wasn't found, please create this file in the same directory as this script $reset_theme"
    # echo $default_hostfile > $hostfile
    exit 1
fi

parameters="$script_dir/parameters.sh"
if [ ! -f $parameters ]; then
    echo -e "$error_theme $script_dir/parameters wasn't found, please create this file in the same directory as this script $reset_theme"
    exit 1
fi


source $parameters

if [ $shr_sleep -le 0 ]; then
    shr_sleep=500000
fi



# nb_nodes=$(ls $hostfile | wc)


# results
total_time_spent=0
total_mypar2=0
nbSAT=0
nbUNSAT=0
nbTIMEOUT=0
nbUNKNOWN=0
start_time=0
end_time=0
result="UNKNOWN"

#duplicate shell 1 in shell 5 so that even after piping to awk the output is printed via shell 5 (which is now the first)
# exec 5>&1

# Init output files
if [ ! -d "./outputs" ]; then
    echo "Creating outputs directory..."
    mkdir outputs
fi

times_file=./outputs/times.csv
if [ ! -f $times_file ]; then
    echo "bin-param,time,myPAR2,nbSAT,nbUNSAT,nbResolved,nbTIMEOUT,nbUNKNOWN,nbUnresolved" >$times_file
fi

# Per configuration metrics
instance_name="L${lstrat}_G${gstrat}_B${nb_bloom_gstrat}${dup}${dist}-$(date +"%d_%m_%Y-%H_%M")"
metric_dir="./outputs/metric_$instance_name"
if [ ! -d $metric_dir ]; then mkdir $metric_dir; fi

# Logs dir in metric folder

# Init output files
if [ ! -d "$metric_dir/logs" ]; then
    echo "Creating outputs directory..."
    mkdir $metric_dir/logs
fi

localSharersInfoFile="$metric_dir/lsharer_L${lstrat}_G${gstrat}_B${nb_bloom_gstrat}${dup}${dist}.csv"
globalSharersInfoFile="$metric_dir/gsharer_L${lstrat}_G${gstrat}_B${nb_bloom_gstrat}${dup}${dist}.csv"
timesPerIntance="$metric_dir/times_L${lstrat}_G${gstrat}_B${nb_bloom_gstrat}${dup}${dist}.csv"

# create/ files if needed
if [ ! -f $localSharersInfoFile ]; then
    echo "nodeID-sharerID,receivedCls,sharedCls,receivedDuplicas" >$localSharersInfoFile
fi

if [ ! -f $globalSharersInfoFile ]; then
    echo "nodeID-sharerID,receivedCls,sharedCls,receivedDuplicas,sharedDuplicasAvoided,messagesSent" >$globalSharersInfoFile
fi

if [ ! -f $timesPerIntance ]; then
    echo "instance,time,myPar2,result,winnerFamily" >$timesPerIntance
fi

cmd="$script_dir/../painless"
args="-v=$verbose -c=$nb_solvers -solver=$solver -t=$timeout -shr-strat=$lstrat -shr-sleep=$shr_sleep $simp $flags"

if [ $gstrat -ge 0 ]; then
    # bind-to none to not have all the threads binded to one core as it is when nb process <= 2
    # map $nb_procs_per_node processes to each node, and each process is bound to $nb_physical_cores hardware threads
    cmd=$(echo -n "mpirun --hostfile $hostfile --bind-to hwthread --map-by ppr:$nb_procs_per_node:node:pe=$nb_physical_cores $cmd")
    args=$(echo -n "$args -gshr-strat=$gstrat -dist")
fi

echo $cmd $args >$metric_dir/info_${gstrat}_${lstrat}.txt

title_in_center "Using Command: ${info_theme}$cmd $args${reset_theme} on input file ${info_theme}$1/${reset_theme}"


for f in $(cat ${input_files}); do
    if [ -f "$f" ]; then

        echo -e "${info_theme}Using file $f${reset_theme}"

        # Getting info from the cnf file
        solution=$(awk 'match($0, /^c NOTE: ([a-zA-z ]*(sat|Sat|SAT)[a-zA-z ]+)/, cap) {print cap[1]}' $f)
        nb_clauses=$(awk 'match($0, /^p cnf [0-9]+ ([0-9]+)/, cap) {print cap[1]}' $f)

        echo -e "${info_theme}Nb Clause = ${nb_clauses}${reset_theme}"

        # Init outputs
        result="UNKNOWN"
        instance=${f##*/}       # extract the file name
        instance=${instance%.*} # remove the .cnf from file name
        output_file="${metric_dir}/logs/log_${instance}.txt"

        eval $cmd $args $f >$output_file

        # Capture solution answered
        results=($(awk 'match($0, /s ([A-Z]+)/, cap) {print cap[1]}' $output_file))
        time_spent=($(awk 'match($0, /Resolution\s*time:\s*([0-9]+.?[0-9]*)\s*s/, cap) {print cap[1]}' $output_file))

        case ${results[0]} in
        "UNKNOWN")
            echo -e "${error_theme} Process returned UNKNOWN!!! ${reset_theme}"
            mypar2=$(echo "($timeout)*2" | bc -l)
            ((nbUNKNOWN++))
            ;;
        "TIMEOUT")
            echo -e "${error_theme} Process interrupted due to timeout! ${reset_theme}"
            mypar2=$(echo "($timeout)*2" | bc -l)
            ((nbTIMEOUT++))
            ;;
        "SATISFIABLE")
            mypar2=$time_spent
            ((nbSAT++))
            ;;
        "UNSATISFIABLE")
            mypar2=$time_spent
            ((nbUNSAT++))
            ;;
        *)
            echo -e "${reset_theme}"
            continue
            ;;
        esac

        echo -e "${log_theme}\t bash captured results: ${results[0]}${reset_theme}" #${results[@]} to debug
        echo -e "${log_theme}\t bash captured time: ${time_spent}${reset_theme}"
        
        echo "$instance,$time_spent,$mypar2,${results[0]}" >>$timesPerIntance

        total_time_spent=$(echo $total_time_spent+$time_spent | bc -l)
        total_mypar2=$(echo $total_mypar2+$mypar2 | bc -l)

        echo -e "\n"

    else
        echo -e "${error_theme}$f is not a file or doesn't exist${reset_theme}"
    fi
done

echo "${instance_name},$total_time_spent,$total_mypar2,$nbSAT,$nbUNSAT,$((nbSAT + nbUNSAT)),$nbTIMEOUT,$nbUNKNOWN,$((nbTIMEOUT + nbUNKNOWN))" >>$times_file

title_in_center "${info_theme}Total time spent for $1 inputs: $total_time_spent seconds${reset_theme}"
// The file is not a cnf but a list of cnfs
// Divided the cnfs into equal numbers of parts
// Use different processes for easy kill on timeout
// For each solver a process that will parse the current file in its queue, solve it, and print solution is launched
// The stdout and stderr of the process is redirected to a log_ and err_ file
// At return of a process save the time and answer of the solver with other stats if needed (to parse log file)
// A csv file is to be generated at the end of the workflow with all the times
#ifndef _forward_h_INCLUDED
#define _forward_h_INCLUDED

struct kissat;

bool
kissat_mab_forward_subsume_temporary(struct kissat*);
void
kissat_mab_forward_subsume_during_elimination(struct kissat*);

#endif

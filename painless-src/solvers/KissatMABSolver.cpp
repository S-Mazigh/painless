// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

// KissatMABSolver includes
extern "C"
{
#include <kissat_mab/src/application.h>
#include <kissat_mab/src/parse.h>
#include <kissat_mab/src/internal.h>
#include <kissat_mab/src/witness.h>
#include <kissat_mab/src/import.h>
}
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Parameters.h"
#include "clauses/ClauseManager.h"
#include "solvers/KissatMABSolver.h"
#include "utils/BloomFilter.h"
#include "painless.h"

// Macros for minisat literal representation conversion
#define MIDX(LIT) (((unsigned)(LIT)) >> 1)
#define MNEGATED(LIT) (((LIT) & 1u))
#define MNOT(LIT) (((LIT) ^ 1u))

void cbkKissatMABSolverExportClause(void *issuer, unsigned int lbd, cvec *cls) // to be changed, maybe use kissat_import, and see what mallob did
{
   KissatMABSolver *mp = (KissatMABSolver *)issuer;

   // if (lbd > mp->lbdLimit || cls->sz > 2)
   if (lbd > mp->lbdLimit)
      return;

   ClauseExchange *ncls = ClauseManager::allocClause(cls->sz);

   ncls->checksum = (cls->sz >= 100 || lbd >= 20) ? -1 : 0;
   for (int i = 0; i < cls->sz; i++)
   {
      int ilit = cvec_data(cls, i);
      int iidx = MIDX(ilit);
      int sign = MNEGATED(ilit); // sign = 1 -; 0 +
      if (ilit & 1)
         assert(sign == 1);
      else
         assert(sign == 0);

      int eidx = PEEK_STACK(mp->solver->exportk, iidx); // exportk[iidx] = eidx
      // std::cout << "ilit: " << ilit << ", Sign: " << sign << ", iidx: " << iidx << " eidx: " << eidx << std::endl;

      // From what i read in import.c it is not always the case:
      //   - in adjust_exports_for_external_literal (kissat * solver, unsigned eidx): unsigned iidx = solver->vars;
      // assert(iidx == eidx - 1); // it is the case only if -simp

      ncls->lits[i] = sign == 1 ? -eidx : eidx;

      if (((int64_t)ncls->checksum) != -1 && i == 0)
      {
         ncls->checksum = lookup3_hash(ncls->lits[i]);
      }
      else if (ncls->checksum != -1)
      {
         ncls->checksum ^= lookup3_hash(ncls->lits[i]);
      }
      // mp->outputf << ncls->lits[i] << " ";
   }
   // mp->outputf << "0" << endl;
   ncls->lbd = lbd;
   ncls->from = mp->id;

   mp->clausesToExport.addClause(ncls);
}

int cbkKissatMABSolverImportUnit(void *issuer)
{
   KissatMABSolver *mp = (KissatMABSolver *)issuer;

   int l = -1;

   ClauseExchange *cls = NULL;

   if (mp->unitsToImport.getClause(&cls) == false)
      return l;

   int eidx = abs(cls->lits[0]);
   import *import = &PEEK_STACK(mp->solver->import, eidx);
   if (import->eliminated)
   {
      l = -10;
   }
   else
   {
      assert(import->imported);
      l = import->lit;
      if (cls->lits[0] < 0)
         l = MNOT(l);
   }
   ClauseManager::releaseClause(cls);

   return l;
}

int cbkKissatMABSolverImportClause(void *issuer, unsigned int *lbd, cvec *mcls)
{
   KissatMABSolver *mp = (KissatMABSolver *)issuer;

   ClauseExchange *cls = NULL;

   if (mp->clausesToImport.getClause(&cls) == false)
      return -1;
   assert(mcls->sz == 0);
   bool alreadySat = false;
   for (size_t i = 0; i < cls->size; i++)
   {
      int eidx = abs(cls->lits[i]);
      import *import = &PEEK_STACK(mp->solver->import, eidx);
      if (import->eliminated)
      {
         alreadySat = true;
      }
      else
      {
         assert(import->imported);
         int ilit = import->lit;
         // assert(ilit == 2 * eidx - 2); // from what i read in import.c eidx == iidx + 1 is not always true
         if (cls->lits[i] < 0)
            ilit = MNOT(ilit);
         cvec_push(mcls, ilit);
      }
   }

   *lbd = cls->lbd;

   ClauseManager::releaseClause(cls);

   if (alreadySat)
      return -10;

   return 1;
}

KissatMABSolver::KissatMABSolver(int id) : SolverInterface(id, KISSAT)
{
   lbdLimit = Parameters::getIntParam("lbd-limit", 2);

   // gid is initialized afterwards (after mpi is initialized)

   solver = kissat_init();
   // outputf.open((to_string(id) + ".txt").c_str());
   solver->cbkExportClause = cbkKissatMABSolverExportClause;
   solver->cbkImportClause = cbkKissatMABSolverImportClause;
   solver->cbkImportUnit = cbkKissatMABSolverImportUnit;
   solver->issuer = this;
   this->gid = id;

   initkissatMABOptions();
}

KissatMABSolver::~KissatMABSolver()
{
   kissat_release(solver);
}

void KissatMABSolver::addOriginClauses(simplify *S)
{
   solver->max_var = S->vars;
   kissat_mab_parse(solver);

   kissat_reserve(solver, S->vars);

   for (int i = 1; i <= S->clauses; i++)
   {
      int l = S->clause[i].size();
      for (int j = 0; j < l; j++)
         kissat_add(solver, S->clause[i][j]);
      kissat_add(solver, 0);
   }
}

bool KissatMABSolver::loadFormula(const char *filename)
{
   kissat_mab_parse(solver);

   strictness strict = NORMAL_PARSING;
   file in;
   uint64_t lineno;
   kissat_open_to_read_file(&in, filename);

   kissat_parse_dimacs(solver, strict, &in, &lineno, &solver->max_var);

   kissat_close_file(&in);

   return true;
}

// Get the number of variables of the formula
int KissatMABSolver::getVariablesCount()
{
   return solver->vars;
}

// Get a variable suitable for search splitting
int KissatMABSolver::getDivisionVariable()
{
   return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void KissatMABSolver::setPhase(const int var, const bool phase)
{
   // TODO use the exportk and imports instead ? and set the values[] ...
   int idx = MIDX(kissat_import_literal(solver, var));
   solver->init_phase[idx] = phase ? 1 : -1;
}

// Bump activity for a given variable
void KissatMABSolver::bumpVariableActivity(const int var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void KissatMABSolver::setSolverInterrupt()
{
   stopSolver = true;

   kissat_terminate(solver);
}

void KissatMABSolver::unsetSolverInterrupt()
{
   stopSolver = false;
}

void KissatMABSolver::setBumpVar(int v)
{
   bump_var = v;
}

void KissatMABSolver::initkissatMABOptions()
{
   LOG(3, "Initializing Kissat configuration ...\n");
   // TODO: Gaussian For restart inc, walkinitially round, ...
   // initial configuration (mostly defaults of kissat_mab)

   // Not Needed in practice
   kissatMABOptions.insert({"check", 1}); // (only on debug) 1: check model only, 2: check redundant clauses
   kissatMABOptions.insert({"quiet", 0});
   kissatMABOptions.insert({"log", 0});
   kissatMABOptions.insert({"verbose", 0}); // verbosity level

   // SAT UNSAT config
   kissatMABOptions.insert({"stable", 1});
   kissatMABOptions.insert({"target", 1});

   // Garbage Collection
   kissatMABOptions.insert({"compact", 1});     // enable compacting garbage collection
   kissatMABOptions.insert({"compactlim", 10}); // compact inactive limit (in percent)

   /*----------------------------------------------------------------------------*/

   // Local search
   kissatMABOptions.insert({"walkinitially", 0}); // the other suboptions are not used
   kissatMABOptions.insert({"walkrounds", 1});    // rounds per walking phase (max is 100000)

   /*----------------------------------------------------------------------------*/

   // (Pre/In)processing Simplifications
   // Xor gates
   kissatMABOptions.insert({"xors", 1});
   kissatMABOptions.insert({"xorsbound", 1});   // minimum elimination bound
   kissatMABOptions.insert({"xorsclsslim", 5}); // xor extraction clause size limit

   kissatMABOptions.insert({"hyper", 1}); // on-the-fly hyper binary resolution
   // Trenary Resolution: maybe its too expensive ??
   kissatMABOptions.insert({"trenary", 1});
   kissatMABOptions.insert({"failed", 1}); // failed literal probing, many suboptions

   // Gates detection
   kissatMABOptions.insert({"and", 1});          // and gates detection
   kissatMABOptions.insert({"equivalences", 1}); // extract and eliminate equivalence gates
   kissatMABOptions.insert({"ifthenelse", 1});   // extract and eliminate if-then-else gates

   kissatMABOptions.insert({"vivify", 1});        // vivify clauses, many suboptions
   kissatMABOptions.insert({"eagersubsume", 20}); // eagerly subsume recently learned clauses
   kissatMABOptions.insert({"reduce", 1});        // learned clause reduction
   kissatMABOptions.insert({"otfs", 1});          // on-the-fly strengthening when deducing learned clause

   // Subsumption options: many other suboptions
   kissatMABOptions.insert({"subsumeclslim", 1e3}); // subsumption clause size limit,

   // Equisatisfiable Simplifications
   kissatMABOptions.insert({"substitute", 1}); // equivalent literal substitution, many subsumptions
   kissatMABOptions.insert({"autarky", 1});    // autarky reasoning
   kissatMABOptions.insert({"eliminate", 1});  // bounded variable elimination (BVE), many suboptions

   // Enhancements for variable elimination
   kissatMABOptions.insert({"backward", 1}); // backward subsumption in BVE
   kissatMABOptions.insert({"forward", 1});  // forward subsumption in BVE
   kissatMABOptions.insert({"extract", 1});  // extract gates in variable elimination

   // Delayed versions
   // kissatMABOptions.insert({"delay", 2}); // maximum delay (autarky, failed, ...)
   // kissatMABOptions.insert({"autarkydelay",1});
   // kissatMABOptions.insert({"trenarydelay", 1});

   /*----------------------------------------------------------------------------*/

   // Search Configuration
   kissatMABOptions.insert({"chrono", 1}); // chronological backtracking
   kissatMABOptions.insert({"chronolevels", 100});

   // CCANR in backtacking
   kissatMABOptions.insert({"ccanr", 0});
   kissatMABOptions.insert({"ccanr_dynamic_bms", 20}); // bms*10 ??
   kissatMABOptions.insert({"ccanr_gap_inc", 1024});   // reducing it make the use of ccanr more frequent

   // Restart Management
   kissatMABOptions.insert({"restart", 1});
   kissatMABOptions.insert({"restartint", 1});     // base restart interval
   kissatMABOptions.insert({"restartmargin", 10}); // fast/slow margin in percent
   kissatMABOptions.insert({"reluctant", 1});      // stable reluctant doubling restarting
   kissatMABOptions.insert({"reducerestart", 0});  // restart at reduce (1=stable , 2=always)

   // Clause and Literal Related Heuristic
   kissatMABOptions.insert({"heuristic", 0}); // 0: VSIDS, 1: CHB
   kissatMABOptions.insert({"mab", 1});       // enable Multi Armed Bandit for VSIDS and CHB switching at restart
   kissatMABOptions.insert({"tier1", 2});     // glue limit for tier1
   kissatMABOptions.insert({"tier2", 6});     // glue limit for tier2

   // Phase
   kissatMABOptions.insert({"phase", 1});
   kissatMABOptions.insert({"phasesaving", 1});
   kissatMABOptions.insert({"rephase", 1});    // reinitialization of decision phases, have two suboptions
   kissatMABOptions.insert({"forcephase", 0}); // force initial phase

   /*----------------------------------------------------------------------------*/

   // not understood, yet
   kissatMABOptions.insert({"probedelay", 0});
   kissatMABOptions.insert({"targetinc", 0});

   // random seed
   kissatMABOptions.insert({"seed", this->gid}); // used in walk and rephase
}

// Diversify the solver
void KissatMABSolver::diversify()
{
   LOG(3, "Diversification of solver %d\n", gid);

   // diversify is called after MPI_Init : TODO, gid in constructor
   if (dist)
   {
      gid = cpus * mpi_rank + id;
      kissatMABOptions.at("seed") = gid;
   }
   // else
   // {
   //    gid = id;
   // }

   if (cpus <= 2)
   {
      LOG(0, "Not enough solvers (%d<=2) to perform a good diversification\n", cpus);
      goto set_options;
   }
   // Across all the solvers in case of dist mode (each mpi process may have a different set of solvers)

   // Focus on UNSAT
   if (gid % 4 == 0)
      kissatMABOptions.at("stable") = 0;

   // Focus on SAT ; target at 2 make the solver better for sat
   else if (gid % 4 == 1)
      kissatMABOptions.at("target") = 2;
   // else default : TARGET = 1, STABLE = 1

   // Xor gates detection is extensively done in solver 7
   if (gid != 7)
   {
      kissatMABOptions.at("xorsclsslim") = 15;
   }

   // Disable chronological backtracking in some solvers
   if (gid % 5 == 0)
   {
      kissatMABOptions.at("chrono") = 0;
   }

   // Some solvers do not do initial walk
   if (gid != 0 && gid % 6 == 0)
   {
      kissatMABOptions.at("walkinitially") = 1;
      kissatMABOptions.at("walkrounds") = gid * 100;
   }

   if (gid != 0 && gid % 10 == 0)
   {
      kissatMABOptions.at("ccanr") = 1;
   }

set_options:
   // Setting the options
   for (auto &opt : kissatMABOptions)
   {
      kissat_set_option(this->solver, opt.first.c_str(), opt.second);
   }

   // printf("Solver with gid=%d\n", gid);
   // print_options(solver);
}

void KissatMABSolver::initshuffle(int id)
{
   kissat_set_option(solver, "initshuffle", 1);
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
KissatMABSolver::solve(const std::vector<int> &cube)
{
   unsetSolverInterrupt();

   // std::vector<ClauseExchange *> tmp;

   // tmp.clear();
   // clausesToAdd.getClauses(tmp);
   // for (size_t ind = 0; ind < tmp.size(); ind++)
   // {
   //    for (size_t i = 0; i < tmp[ind]->size; i++)
   //       kissat_add(solver, tmp[ind]->lits[i]);
   //    kissat_add(solver, 0);
   //    ClauseManager::releaseClause(tmp[ind]);
   // }
   // printf("KissatMABSolver::solve() 1 for solver=%p with &statistics=%p searches@%p=%ld\n", solver, &(solver->statistics), &(solver->statistics.searches), solver->statistics.searches);
   // Ugly fix, to be enhanced.
   this->solver->statistics.searches = 0; // since SequentialWorker loop until force or solution, we must do this to not get "incremental not supported error".
   // printf("KissatMABSolver::solve() 2 for solver=%p with &statistics=%p \n", solver, &(solver->statistics), &(solver->statistics.searches));
   assert(cube.size() == 0);
   // printf("KissatMABSolver::solve() 3 for solver=%p with &statistics=%p \n", solver, &(solver->statistics), &(solver->statistics.searches));
   int res = kissat_solve(solver);
   // printf("KissatMABSolver::solve() for solver=%p returning %d (SAT=10, UNSAT=20)\n", solver, res);
   if (res == 10)
   {
      LOG(1, "Solver %d responded with SAT.\n",this->gid);
      #ifndef NDEBUG
      kissat_check_satisfying_assignment(solver);
      #endif
      return SAT;
   }
   if (res == 20){
      LOG(1, "Solver %d responded with UNSAT.\n",this->gid);
      return UNSAT;
   }
   LOG(1, "Solver %d responded with UNKNOWN.\n",this->gid);
   return UNKNOWN;
}

void KissatMABSolver::addClause(ClauseExchange *clause)
{
}

bool KissatMABSolver::importClause(ClauseExchange *clause)
{
   if (clause->size == 1)
   {
      unitsToImport.addClause(clause);
   }
   else
   {
      clausesToImport.addClause(clause);
   }
   ClauseManager::increaseClause(clause);
   return true;
}

void KissatMABSolver::addClauses(const std::vector<ClauseExchange *> &clauses)
{
}

void KissatMABSolver::addInitialClauses(const std::vector<ClauseExchange *> &clauses)
{
}

void KissatMABSolver::importClauses(const std::vector<ClauseExchange *> &clauses)
{
   for (auto cls : clauses)
   {
      importClause(cls);
   }
}

void KissatMABSolver::exportClauses(std::vector<ClauseExchange *> &clauses)
{
   clausesToExport.getClauses(clauses);
}

void KissatMABSolver::increaseClauseProduction()
{
   lbdLimit++;
}

void KissatMABSolver::decreaseClauseProduction()
{
   if (lbdLimit > 2)
   {
      lbdLimit--;
   }
}

SolvingStatistics
KissatMABSolver::getStatistics()
{
   SolvingStatistics stats;

   stats.conflicts = solver->statistics.conflicts;
   stats.propagations = solver->statistics.propagations;
   stats.restarts = solver->statistics.restarts;
   stats.decisions = solver->statistics.decisions;
   stats.memPeak = 0;

   return stats;
}

std::vector<int>
KissatMABSolver::getModel()
{
   std::vector<int> model;

   for (int i = 1; i <= solver->max_var; i++)
   {
      int tmp = kissat_value(solver, i);
      if (!tmp)
         tmp = i;
      model.push_back(tmp);
   }

   return model;
}

std::vector<int>
KissatMABSolver::getFinalAnalysis()
{
   std::vector<int> outCls;
   return outCls;
}

std::vector<int>
KissatMABSolver::getSatAssumptions()
{
   std::vector<int> outCls;
   return outCls;
};

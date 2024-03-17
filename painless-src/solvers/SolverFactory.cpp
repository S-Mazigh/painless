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

#include "solvers/KissatMABSolver.h"
#include "solvers/SolverFactory.h"
#include "solvers/Reducer.h"
#include "solvers/MapleCOMSPSSolver.h"
#include "utils/Parameters.h"
#include "utils/System.h"

void SolverFactory::sparseRandomDiversification(
    const std::vector<SolverInterface *> &solvers)
{
   if (solvers.size() == 0)
      return;

   int vars = solvers[0]->getVariablesCount();

   // The first solver of the group (1 LRB/1 VSIDS) keeps polarity = false for all vars
   for (int sid = 1; sid < solvers.size(); sid++)
   {
      // srand(sid);
      for (int var = 1; var <= vars; var++)
      {
         if (rand() % solvers.size() == 0)
         {
            solvers[sid]->setPhase(var, rand() % 2 == 1);
         }
      }
   }
}

void SolverFactory::diversification(const std::vector<SolverInterface *> &solvers)
{
   LOG(2, "Diversification: \n");
   for (auto solver : solvers)
   {
      solver->diversify();
   }
}

void SolverFactory::initshuffleDiversification(const std::vector<SolverInterface *> &solvers)
{
   for (int sid = 0; sid < solvers.size(); sid++)
   {
      solvers[sid]->initshuffle(sid);
   }
}

SolverInterface *
SolverFactory::createKissatMABSolver()
{
   int id = currentIdSolver.fetch_add(1);
   SolverInterface *solver = new KissatMABSolver(id);

   // solver->loadFormula(Parameters::getFilename()); // done in main by readWorker

   return solver;
}

void SolverFactory::createKissatMABSolvers(int maxSolvers,
                                             std::vector<SolverInterface *> &solvers)
{
   for (size_t i = 0; i < maxSolvers; i++)
   {
      KissatMABSolver *kissat = (KissatMABSolver *)createKissatMABSolver();
      solvers.push_back(kissat);
   }
}

SolverInterface *
SolverFactory::createReducerSolver()
{
   int id = currentIdSolver.fetch_add(1);

   SolverInterface *solver = new Reducer(id, SolverFactory::createMapleCOMSPSSolver());

   return solver;
}

SolverInterface *
SolverFactory::createMapleCOMSPSSolver()
{
   int id = currentIdSolver.fetch_add(1);

   SolverInterface *solver = new MapleCOMSPSSolver(id);

   return solver;
}

// Distributed versions
#ifndef NDIST

void SolverFactory::sparseRandomDiversification(
    const vector<SolverInterface *> &solvers, int mpi_rank, int ncpu)
{
   if (solvers.size() == 0)
      return;

   int vars = solvers[0]->getVariablesCount();

   // The first solver of the group (1 LRB/1 VSIDS) keeps polarity = false for all vars
   for (int sid = 1; sid < solvers.size(); sid++)
   {
      srand(ncpu * mpi_rank + sid);
      for (int var = 1; var <= vars; var++)
      {
         if (rand() % solvers.size() == 0)
         {
            solvers[sid]->setPhase(var, rand() % 2 == 1);
         }
      }
   }
}

void SolverFactory::initshuffleDiversification(const vector<SolverInterface *> &solvers, int mpi_rank, int ncpu)
{
   for (int sid = 0; sid < solvers.size(); sid++)
   {
      /* TODO: check the correctness of the formula
         ncpu * mpi_rank + solver_id
      */
      solvers[sid]->initshuffle(ncpu * mpi_rank + sid);
   }
}
#endif

void SolverFactory::printStats(const std::vector<SolverInterface *> &solvers)
{
   printf("c | ID | conflicts  | propagations |  restarts  | decisions  "
          "| memPeak |\n");

   for (size_t i = 0; i < solvers.size(); i++)
   {
      SolvingStatistics stats = solvers[i]->getStatistics();

      printf("c | %2zu | %10ld | %12ld | %10ld | %10ld | %7d |\n",
             solvers[i]->id, stats.conflicts, stats.propagations,
             stats.restarts, stats.decisions, (int)stats.memPeak);
   }
   for (SolverInterface *s : solvers)
   {
      // if (s->testStrengthening())
      s->getStatistics();
   }
}

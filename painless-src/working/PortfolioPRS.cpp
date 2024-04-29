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

#include "utils/Parameters.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "working/PortfolioPRS.h"
#include "working/SequentialWorker.h"
#include "painless.h"
#include <thread>

#include "solvers/SolverFactory.h"
#include "clauses/ClauseDatabaseLockFree.h"
#include "sharing/SharingStrategyFactory.h"

PortfolioPRS::PortfolioPRS()
{
#ifndef NDIST
   if (dist)
   {
      int maxClauseSize = Parameters::getIntParam("maxClauseSize", 50);
      this->globalDatabase = std::make_shared<GlobalDatabase>(currentIdSolver.fetch_add(1), std::make_shared<ClauseDatabaseLockFree>(maxClauseSize), std::make_shared<ClauseDatabaseLockFree>(maxClauseSize));
   }
#endif
}

PortfolioPRS::~PortfolioPRS()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      delete slaves[i];
   }
}

void PortfolioPRS::solve(const std::vector<int> &cube)
{
   LOG(">> PortfolioPRS");
   std::vector<simpleClause> initClauses;
   std::vector<std::thread> clausesLoad;

   std::vector<std::shared_ptr<SolverCdclInterface>> cdclSolvers;
   std::vector<std::shared_ptr<LocalSearchSolver>> localSolvers;

   int nbKissat = 9;
   int nbGaspi = 5;

   strategyEnding = false;
   /* PRS */
   int res = prs.do_preprocess(Parameters::getFilename());
   LOGDEBUG1("PRS returned %d", res);
   if (20 == res)
   {
      LOG("PRS answered UNSAT");
      finalResult = SatResult::UNSAT;
      this->join(this, finalResult, {});
      return;
   }
   else if (10 == res)
   {
      LOG("PRS answered SAT");
      for (int i = 1; i <= prs.vars; i++)
      {
         finalModel.push_back(prs.model[i]);
      }
      finalResult = SatResult::SAT;
      this->join(this, finalResult, finalModel);
      return;
   }

   // Free some memory
   prs.release_most();

   // PRS uses indexes from 1 to prs.clauses
   initClauses = std::move(prs.clause);
   initClauses.erase(initClauses.begin());

   for (int i = 0; i < nbKissat; i++)
      SolverFactory::createSolver('k', cdclSolvers, localSolvers);
   for (int i = 0; i < nbGaspi; i++)
      SolverFactory::createSolver('K', cdclSolvers, localSolvers);
   // one yalsat or kissat_mab
   if(initClauses.size() < 30 * MILLION)
      SolverFactory::createSolver('y', cdclSolvers, localSolvers);
   else
      SolverFactory::createSolver('k', cdclSolvers, localSolvers);

   SolverFactory::diversification(cdclSolvers, localSolvers, Parameters::getBoolParam("dist"), mpi_rank, world_size);

   for (auto cdcl : cdclSolvers)
   {
      clausesLoad.emplace_back(&LocalSearchSolver::addInitialClauses, cdcl, std::ref(initClauses), prs.vars);
   }
   for (auto local : localSolvers)
   {
      clausesLoad.emplace_back(&LocalSearchSolver::addInitialClauses, local, std::ref(initClauses), prs.vars);
   }

   /* Sharing */
   /* ------- */

   if (dist)
   {
      // this->globalDatabase->setMaxVar(this->beforeSbvaVarCount); // doesn't make the use of asynchronous sbva correct (all processes must choose same config: no local search)
      /* Init LocalStrategy */
      SharingStrategyFactory::instantiateLocalStrategies(Parameters::getIntParam("shr-strat", 1), this->localStrategies, {this->globalDatabase}, cdclSolvers);
      /* Init GlobalStrategy */
      SharingStrategyFactory::instantiateGlobalStrategies(Parameters::getIntParam("gshr-strat", 2), {this->globalDatabase}, globalStrategies);
   }
   else
   {
      SharingStrategyFactory::instantiateLocalStrategies(Parameters::getIntParam("shr-strat", 1), this->localStrategies, {}, cdclSolvers);
   }

   std::vector<std::shared_ptr<SharingStrategy>> sharingStrategiesConcat;

   /* Launch sharers */
   for (auto lstrat : localStrategies)
   {
      sharingStrategiesConcat.push_back(lstrat);
   }
   for (auto gstrat : globalStrategies)
   {
      sharingStrategiesConcat.push_back(gstrat);
   }

   SharingStrategyFactory::launchSharers(sharingStrategiesConcat, this->sharers);

   sharingStrategiesConcat.clear();

   // Wait for clauses init
   for (auto &worker : clausesLoad)
      worker.join();

   clausesLoad.clear();

   if (globalEnding)
   {
      this->setInterrupt();
      return;
   }

   initClauses.clear();

   for (auto cdcl : cdclSolvers)
   {
      this->addSlave(new SequentialWorker(cdcl));
   }
   for (auto local : localSolvers)
   {
      this->addSlave(new SequentialWorker(local));
   }

   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->solve(cube);
   }
}

void PortfolioPRS::join(WorkingStrategy *strat, SatResult res,
                        const std::vector<int> &model)
{
   if (res == UNKNOWN || strategyEnding || globalEnding)
      return;

   strategyEnding = true;

   setInterrupt();

   if (parent == NULL)
   { // If it is the top strategy
      globalEnding = true;
      finalResult = res;

      if (res == SAT)
      {
         finalModel = model; /* Make model non const for safe move ? */
         /* remove potential sbva added variables */
         prs.restoreModel(finalModel);
      }
      if (strat != this)
      {
         SequentialWorker *winner = (SequentialWorker *)strat;
         winner->solver->printWinningLog();
      }
      else
      {
         LOGSTAT("The winner is of type: PRS-PRE");
      }
      pthread_mutex_lock(&mutexGlobalEnd);
      pthread_cond_broadcast(&condGlobalEnd);
      pthread_mutex_unlock(&mutexGlobalEnd);
      LOGDEBUG1("Broadcasted the end");
   }
   else
   { // Else forward the information to the parent strategy
      parent->join(this, res, model);
   }
}

void PortfolioPRS::setInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->setInterrupt();
   }
}

void PortfolioPRS::unsetInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->unsetInterrupt();
   }
}

void PortfolioPRS::waitInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->waitInterrupt();
   }
}

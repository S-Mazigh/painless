
#include "utils/Parameters.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "working/PortfolioSimple.h"
#include "working/SequentialWorker.h"
#include "painless.h"
#include <thread>

#include "solvers/SolverFactory.h"
#include "clauses/ClauseDatabaseLockFree.h"
#include "sharing/SharingStrategyFactory.h"

PortfolioSimple::PortfolioSimple()
{
#ifndef NDIST
   if (dist)
   {
      int maxClauseSize = Parameters::getIntParam("maxClauseSize", 50);
      /* The ids are to be managed separately, idDiversification, and idSharingEntity, Temp: cpus is surely not used by the solvers */
      this->globalDatabase = std::make_shared<GlobalDatabase>(std::make_shared<ClauseDatabaseLockFree>(maxClauseSize), std::make_shared<ClauseDatabaseLockFree>(maxClauseSize));
   }
#endif
}

PortfolioSimple::~PortfolioSimple()
{
   for (int i = 0; i < this->sharers.size(); i++)
      this->sharers[i]->printStats();
   this->globalDatabase->printStats();
   for (size_t i = 0; i < slaves.size(); i++)
   {
      delete slaves[i];
   }
}

void PortfolioSimple::solve(const std::vector<int> &cube)
{
   LOG(">> PortfolioSimple");

   std::vector<std::shared_ptr<SolverCdclInterface>> cdclSolvers;
   std::vector<std::shared_ptr<LocalSearchInterface>> localSolvers;

   strategyEnding = false;

   std::vector<simpleClause> initClauses;
   unsigned varCount;

   if (mpi_rank <= 0)
      parseFormula(Parameters::getFilename(), initClauses, &varCount);

   LOG("Main Memory After Parse : %f / %f", MemInfo::getUsedMemory(), MemInfo::getAvailableMemory());
   // Send instance via MPI from leader 0 to workers.
   if (dist)
   {
      sendFormula(initClauses, &varCount, 0);
   }

   LOG("Main Memory After Send Formula: %f / %f", MemInfo::getUsedMemory(), MemInfo::getAvailableMemory());

   SolverFactory::createInitSolvers(cpus, Parameters::getParam("solver"), cdclSolvers, localSolvers, initClauses, varCount);

   initClauses.clear();

   LOG("Main Memory After solver factory: %f / %f", MemInfo::getUsedMemory(), MemInfo::getAvailableMemory());

   /* Sharing */
   /* ------- */

   if (dist)
   {
      SharingStrategyFactory::instantiateLocalStrategies(Parameters::getIntParam("shr-strat", 1), this->localStrategies, {this->globalDatabase}, cdclSolvers);
      SharingStrategyFactory::instantiateGlobalStrategies(Parameters::getIntParam("gshr-strat", 2), {this->globalDatabase}, globalStrategies);
   }
   else
   {
      SharingStrategyFactory::instantiateLocalStrategies(Parameters::getIntParam("shr-strat", 1), this->localStrategies, {}, cdclSolvers);
   }

   std::vector<std::shared_ptr<SharingStrategy>> sharingStrategiesConcat;

   sharingStrategiesConcat.clear();

   LOG1("All solvers loaded the clauses");

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

   if (globalEnding)
   {
      this->setInterrupt();
      return;
   }

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

   LOG("Main Memory Before Leaving: %f / %f", MemInfo::getUsedMemory(), MemInfo::getAvailableMemory());
}

void PortfolioSimple::join(WorkingStrategy *strat, SatResult res,
                           const std::vector<int> &model)
{
   if (res == SatResult::UNKNOWN || strategyEnding)
      return;

   strategyEnding = true;

   setInterrupt();

   if (parent == NULL)
   { // If it is the top strategy
      globalEnding = true;
      finalResult = res;

      if (res == SatResult::SAT)
      {
         finalModel = model;
      }

      if (strat != this)
      {
         SequentialWorker *winner = (SequentialWorker *)strat;
         winner->solver->printWinningLog();
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

void PortfolioSimple::setInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->setInterrupt();
   }
}

void PortfolioSimple::unsetInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->unsetInterrupt();
   }
}

void PortfolioSimple::waitInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->waitInterrupt();
   }
}

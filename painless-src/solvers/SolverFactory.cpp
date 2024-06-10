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
#include "solvers/SolverFactory.h"
#include "utils/Parameters.h"
#include "utils/System.h"
#include "utils/Logger.h"
#include "utils/ErrorCodes.h"
#include "utils/MpiUtils.h"
#include "painless.h"

#include "solvers/CDCL/KissatMABSolver.h"
#include "solvers/CDCL/KissatGASPISolver.h"
#include "solvers/CDCL/MapleCOMSPSSolver.h"
#include "solvers/LocalSearch/YalSatSolver.h"

#include <cassert>
#include <random>
#include <map>

std::atomic<int> SolverFactory::currentIdSolver(0);

void SolverFactory::diversification(const std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, const std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers)
{
   if (dist)
   {
      if (mpi_rank < 0)
      {
         LOGERROR("MPI was not initialized");
         exit(PERR_MPI);
      }
   }
   /* More flexibile diversification */
   unsigned nb_solvers = cdclSolvers.size(); // cdclSolvers.size() + localSolvers.size();
   unsigned maxId = (dist) ? nb_solvers * world_size : nb_solvers;

      if (maxId <= 2)
   {
      LOGWARN("Not enough solvers (%d) to perform a good diversification (need >= 2)", maxId);
   }

   // std::random_device uses /dev/urandom by default (to be checked)
   std::mt19937 engine(std::random_device{}());
   std::uniform_int_distribution<int> uniform(1, Parameters::getIntParam("max-div-noise", 100)); /* test with a normal dist*/

   LOGDEBUG1("Diversification on: nb_solvers = %d, dist = %d, mpi_rank = %d, world_size = %d, maxId = %d", nb_solvers, dist.load(), mpi_rank, world_size, maxId);

   for (auto cdclSolver : cdclSolvers)
   {

      cdclSolver->diversify(engine, uniform);
      // solver->printParameters();
   }

   for (auto localSolver : localSolvers)
   {
      localSolver->diversify(engine, uniform);
   }

   LOG("Diversification done");
}

SolverAlgorithmType SolverFactory::createSolver(char type, std::shared_ptr<SolverInterface> &createdSolver)
{
   int id = currentIdSolver.fetch_add(1);
   LOGDEBUG1("Creating Solver %d", id);
   if (id >= cpus)
   {
      LOGWARN("Solver of type '%c' will not be instantiated, the number of solvers %d reached the maximum %d.", type, id, cpus);
      return SolverAlgorithmType::UNKNOWN;
   }
   if (dist)
   {
      id = cpus * mpi_rank + id;
   }
   switch (type)
   {
#ifdef GLUCOSE_
   case 'g':
      // GLUCOSE implementation
      return SolverAlgorithmType::CDCL;
      // break;
#endif

#ifdef LINGELING_
   case 'l':
      // LINGELING implementation
      return SolverAlgorithmType::CDCL;
      // break;
#endif

#ifdef MAPLECOMSPS_
   case 'M':
      createdSolver = std::make_shared<MapleCOMSPSSolver>(id, MapleCOMSPSSolver::mapleCount);
      MapleCOMSPSSolver::mapleCount++;

      return SolverAlgorithmType::CDCL;
      // break;
#endif

#ifdef MINISAT_
   case 'm':
      // MINISAT implementation
      return SolverAlgorithmType::CDCL;
      // break;
#endif

#ifdef KISSATMAB_
   case 'k':
      createdSolver = std::make_shared<KissatMABSolver>(id);
      return SolverAlgorithmType::CDCL;
      // break;
#endif

#ifdef KISSATGASPI_
   case 'K':
      createdSolver = std::make_shared<KissatGASPISolver>(id);
      return SolverAlgorithmType::CDCL;
      // break;
#endif

#ifdef YALSAT_
   case 'y':
      createdSolver = std::make_shared<YalSat>(id);
      return SolverAlgorithmType::LOCAL_SEARCH;
      // break;
#endif

   default:
      LOGERROR("The SolverCdclType %d specified is not available!", type);
      exit(PERR_UNKNOWN_SOLVER);
   }
}

SolverAlgorithmType SolverFactory::createSolver(char type, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers)
{
   std::shared_ptr<SolverInterface> createdSolver;
   switch (createSolver(type, createdSolver))
   {
   case SolverAlgorithmType::CDCL:
      cdclSolvers.push_back(std::static_pointer_cast<SolverCdclInterface>(createdSolver));
      return SolverAlgorithmType::CDCL;

   case SolverAlgorithmType::LOCAL_SEARCH:
      localSolvers.push_back(std::static_pointer_cast<LocalSearchInterface>(createdSolver));
      return SolverAlgorithmType::LOCAL_SEARCH;

   default: /* Unknown type case is dealed in base function */
      return SolverAlgorithmType::UNKNOWN;
   }
}

void SolverFactory::createSolvers(int maxSolvers, std::string portfolio, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers)
{
   unsigned typeCount = portfolio.size();
   for (size_t i = 0; i < maxSolvers; i++)
   {
      createSolver(portfolio.at(i % typeCount), cdclSolvers, localSolvers);
   }
}

void SolverFactory::createInitSolvers(int count, std::string portfolio, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers, std::vector<simpleClause> &initClauses, unsigned varCount)
{
   // Create a map of char to unsigned for count per solver type
   std::map<char, unsigned> countPerSolver;
   std::vector<std::thread> clauseLoaders;
   unsigned portfolioSize = portfolio.size();

   std::mt19937 engine(std::random_device{}());
   std::uniform_int_distribution<int> uniform(1, Parameters::getIntParam("max-div-noise", 100));

   if (!portfolioSize)
   {
      LOGERROR("Empty portfolio string");
      exit(PERR_ARGS_ERROR);
   }
   else if (count < portfolioSize)
   {
      LOGERROR("Specified portfolio requires at least %d threads (-c=%d)", portfolioSize, cpus);
      exit(PERR_ARGS_ERROR);
   }

   unsigned remainder = count % portfolioSize;

   std::pair<std::map<char, unsigned int>::iterator, bool> retPair;
   for (char &solverType : portfolio)
   {
      retPair = countPerSolver.insert({solverType, count / portfolioSize});
      if (!retPair.second)
         countPerSolver.at(solverType) += count / portfolioSize;
   }

   for (unsigned i = 0; i < remainder; i++)
      countPerSolver.at(portfolio.at(i % portfolioSize))++;

   // Loop on portfolio string: 1- create one instance, 2- add initialClauses, 3- count number of possible instantiations
   double initMem, finalMem, diffMem;
   double availableMem, neededMem;
   unsigned possibleCount;

   neededMem = 0;
   for (auto &pair : countPerSolver)
   {
      LOGDEBUG1("Initial Count of %c: %u", pair.first, countPerSolver.at(pair.first));

      initMem = MemInfo::getUsedMemory();

      switch (SolverFactory::createSolver(pair.first, cdclSolvers, localSolvers))
      {
      case SolverAlgorithmType::CDCL:
         cdclSolvers.back()->diversify(engine, uniform);
         cdclSolvers.back()->addInitialClauses(initClauses, varCount);
         break;

      case SolverAlgorithmType::LOCAL_SEARCH:
         localSolvers.back()->diversify(engine, uniform);
         localSolvers.back()->addInitialClauses(initClauses, varCount);
         break;

      case SolverAlgorithmType::UNKNOWN:
         LOG1("Max Id reached, factory returning");
         goto end;

      default:
         LOGERROR("The solver algorithm type is not supported by the factory!");
         exit(PERR_NOT_SUPPORTED);
      }

      finalMem = MemInfo::getUsedMemory();

      diffMem = (finalMem - initMem) * 5 ; /* *5 just a guardrail for big instances */
      // neededMem +=diffMem; /*getAvailableMemory() is aware*/

      availableMem = MemInfo::getAvailableMemory() - neededMem;
      possibleCount = availableMem / diffMem;

      LOGDEBUG1("Solver type %c needs %f Ko of Memory. With %f Ko available memory, we can create %u other such solvers.", pair.first, diffMem, availableMem, possibleCount);

      neededMem += diffMem * (pair.second - 1);

      if (neededMem >= availableMem)
      {
         LOGWARN("Portfolio %s stopping at solver type %c (%u / %u)", portfolio.c_str(), pair.first, possibleCount + 1, pair.second);
         pair.second = (possibleCount + 1);
         break;
      }
   }

   // Multi thread addinitClauses on the other solvers
   for (auto &pair : countPerSolver)
   {
      LOG1("SolverType %c : %u", pair.first, pair.second);
      for (unsigned i = pair.second; i > 1; i--) /* since first is init in previous loop */
      {
         LOGDEBUG1("%d : creating %c", i, pair.first);
         switch (SolverFactory::createSolver(pair.first, cdclSolvers, localSolvers))
         {
         case SolverAlgorithmType::CDCL:
            cdclSolvers.back()->diversify(engine, uniform);
            clauseLoaders.emplace_back(&SolverCdclInterface::addInitialClauses, cdclSolvers.back(), std::ref(initClauses), varCount);
            break;

         case SolverAlgorithmType::LOCAL_SEARCH:
            localSolvers.back()->diversify(engine, uniform);
            clauseLoaders.emplace_back(&LocalSearchInterface::addInitialClauses, localSolvers.back(), std::ref(initClauses), varCount);
            break;

         case SolverAlgorithmType::UNKNOWN:
            LOG1("Max Id reached, factory returning");
            goto end;

         default:
            LOGERROR("An unsupported algorithm type was created !");
            exit(PERR_NOT_SUPPORTED);
         }
      }
   }

end:
   for (auto &thread : clauseLoaders)
      thread.join();
   LOG("All instanced solvers loaded the initial clauses");
}

void SolverFactory::printStats(const std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, const std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers)
{
   /*LOGSTAT(" | ID | conflicts  | propagations |  restarts  | decisions  "
       "| memPeak |");*/

   for (auto s : cdclSolvers)
   {
      // if (s->testStrengthening())
      s->printStatistics();
   }

   /*LOGSTAT(" | ID | flips  | unsatClauses |  restarts | memPeak |");*/
   for (auto s : localSolvers)
   {
      // if (s->testStrengthening())
      s->printStatistics();
   }
}

// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
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

#include "painless.h"

#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/System.h"
#include "utils/SatUtils.h"

#include "solvers/SolverFactory.h"
#include "clauses/ClauseManager.h"
#include "sharing/LocalStrategies/HordeSatSharing.h"
#include "sharing/LocalStrategies/HordeSatSharingAlt.h"
#include "sharing/LocalStrategies/HordeStrSharing.h"
#include "sharing/LocalStrategies/SimpleSharing.h"

#include "sharing/Sharer.h"
#include "sharing/SharerMulti.h"
#include "working/SequentialWorker.h"
#include "working/Portfolio.h"
#include "simplify/simplify.h"

#ifndef NDIST
#include "sharing/GlobalStrategies/AllGatherSharing.h"
#include "sharing/GlobalStrategies/MallobSharing.h"
#include "sharing/GlobalStrategies/RingSharing.h"
#include "sharing/GlobalDatabase.h"
#include "clauses/ClauseDatabaseLockFree.h"
#endif

#include <unistd.h>
#include <random>

// -------------------------------------------
// Declaration of global variables
// -------------------------------------------

atomic<bool> globalEnding(false);
pthread_mutex_t mutexGlobalEnd;
pthread_cond_t condGlobalEnd;

std::vector<Sharer *> sharers;

// int nSharers = 0;

WorkingStrategy *working = NULL;

#ifndef NDIST
int mpi_rank = -1;
int world_size = -1;
#else
int mpi_rank = 0;
int world_size = 0;
#endif

bool dist = false;

// needed for diversification
int nb_groups;
int cpus;

SatResult finalResult = UNKNOWN;

std::vector<int> finalModel;
simplify *S;

void *readWorker(void *arg)
{
   SolverInterface *sq = (SolverInterface *)arg;
   if (Parameters::getBoolParam("simp"))
      sq->addOriginClauses(S);
   else
      sq->loadFormula(Parameters::getFilename());
   return NULL;
}

// -------------------------------------------
// Main of the framework
// -------------------------------------------
int main(int argc, char **argv)
{
   Parameters::init(argc, argv);

   if (Parameters::getFilename() == NULL ||
       Parameters::getBoolParam("h"))
   {
      cout << "USAGE: " << argv[0] << " [options] input.cnf" << endl;
      cout << "Options:" << endl;
      cout << "General:" << endl;
      cout << "\t-c=<INT>\t\t number of cpus, default is 24" << endl;
      // cout << "\t-max-memory=<INT>\t memory limit in GB, default is 200" << endl;
      cout << "\t-t=<INT>\t\t timeout in seconds, default is no limit" << endl;
      cout << "\t-v=<INT>\t\t verbosity level, default is 0" << endl;
      cout << "Local Sharing:" << endl;
      cout << "\t-dup\t\t enable duplicates detection if supported by the local sharing strategy choosen" << endl;
      cout << "\t-lbd-limit=<INT>\t LBD limit of exported clauses, default is 2" << endl;
      cout << "\t-shr-strat=<INT>\t\t strategy for local sharing value in [1,5], 0 by default" << endl;
      cout << "\t-shr-sleep=<INT>\t time in useconds a sharer sleep eachround, default is 500000 (0.5s)" << endl;
      cout << "\t-shr-lit=<INT>\t\t number of literals shared per round, default is 1500" << endl;
      cout << "Global Sharing:" << endl;
      cout << "\t-dist\t\t enable distributed(global) sharing, disabled by default" << endl;
      cout << "\t-gshr-lit=<INT>\t\t number of literals (+metadata) shared per round in the global sharing strategy, default is 1500*c" << endl;
      cout << "\t-gshr-strat=<INT>\t\t global sharing strategy, by default 1 (allgather)" << endl;
      cout << "\t-max-cls-size=<INT>\t\t the maximum size of clauses to be added to the limited databases, eg: ClauseDatabaseLmited used in GlobalDatabase" << endl;
      cout << "Preprocessing and diversification:" << endl;
      cout << "\t-simp\t\t simplify" << endl;
      cout << "\t-initshuffle\t\t initshuffle" << endl;
      return 0;
   }

   nb_groups = Parameters::getIntParam("groups", 2);
   cpus = Parameters::getIntParam("c", 30);
   dist = Parameters::getBoolParam("dist");
   int shr_strat = Parameters::getIntParam("shr-strat", 0);
   string solverName = Parameters::getParam("solver", "k");
   setVerbosityLevel(Parameters::getIntParam("v", 0));

#ifndef NDIST
   if (Parameters::getBoolParam("gtest"))
   {
      int maxClauseSize = Parameters::getIntParam("maxClauseSize", 50);
      GlobalDatabase *test_database;

      MPI_Init(NULL, NULL);
      if (Parameters::getBoolParam("one-sharer"))
      {
         test_database = new GlobalDatabase(-1, new ClauseDatabaseVector(maxClauseSize), new ClauseDatabaseVector(maxClauseSize));
      }
      else // at least two threads on globaldatabase
      {
         test_database = new GlobalDatabase(-1, new ClauseDatabaseLockFree(maxClauseSize), new ClauseDatabaseLockFree(maxClauseSize));
      }
      LOG(0, "Testing integrity of serial/desrial of global strategies...\n");

      LOG(0, "AllGather resulted in  %d\n", AllGatherSharing(-1, test_database).testIntegrity());
      test_database->clear();
      
      LOG(0, "Mallob resulted in  %d\n", MallobSharing(-1, test_database).testIntegrity());
      test_database->clear();
      
      LOG(0, "Ring resulted in  %d\n", RingSharing(-1, test_database).testIntegrity());
      
      MPI_Finalize();
      exit(0);
   }
#endif

   // Create and init solvers
   std::vector<SolverInterface *> solvers;
   std::vector<SolverInterface *> solvers_VSIDS;
   std::vector<SolverInterface *> solvers_LRB;
   std::vector<SolverInterface *> reducers;

   SolverFactory::createKissatMABSolvers(cpus, solvers);

   int nSolvers = solvers.size();
   int nextId = nSolvers;

   if (Parameters::getBoolParam("simp"))
   {
      S = new simplify();
      S->readfile(Parameters::getFilename());
      // printf("c finish read, var: %d, clause: %d, use: %.2lf s\n", S->vars, S->clauses, getRelativeTime());
      S->simplify_init();
      int res = S->simplify_easy_clause();
      if (!res)
      {
         S->release();
         delete[] S->mapto;
         delete[] S->mapval;
         S->clause.clear(true);
         delete S;
         cout << "s UNSATISFIABLE" << endl;
         return 0;
      }
      // if (S->clauses <= 15000000) {
      res = S->simplify_resolution();
      if (!res)
      {
         S->release();
         delete[] S->mapto;
         delete[] S->mapval;
         S->clause.clear(true);
         S->res_clause.clear(true);
         S->resolution.clear(true);
         delete S;
         cout << "s UNSATISFIABLE" << endl;
         return 0;
      }
      // }
      if (S->clauses <= 15000000)
      {
         res = S->simplify_binary();
         if (!res)
         {
            S->release();
            delete[] S->mapto;
            delete[] S->mapval;
            S->clause.clear(true);
            S->res_clause.clear(true);
            S->resolution.clear(true);
            delete S;
            cout << "s UNSATISFIABLE" << endl;
            return 0;
         }
      }

      LOG(0, "Simplify Done. Size reduction: \n\t-Clauses: from %d to %d.\n\t-Variables: from %d to %d.",S->oriclauses, S->clauses,S->orivars, S->vars);
      srand(S->vars);
      S->release();
   }

   std::vector<SharingStrategy *> sharingStrategies;

#ifndef NDIST
   // GlobalStrategy initialization
   GlobalSharingStrategy *g_strat;
   GlobalDatabase *globalDatabase;
   if (dist)
   {
      // MPI Initialization
      // If MPI_Init_Thread causes problems see how to wait for the mpi_rank
      int provided;
      MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &provided);
      MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

      LOG(2, "Thread strategy provided is %d\n", provided);

      if (provided < MPI_THREAD_SERIALIZED)
      {
         LOG(0, "Error, MPI initialization is not possible !!!");
         dist = false;
         goto sharersInit;
      }
      if (MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank) != MPI_SUCCESS)
      {
         LOG(0, "Error at MPI rank get !!\n");
      }
      else
      {
         char hostname[256];
         gethostname(hostname, sizeof(hostname));
         printf("d PID %d on %s is of rank %d\n", getpid(), hostname, mpi_rank);
      }
      if (MPI_Comm_size(MPI_COMM_WORLD, &world_size) != MPI_SUCCESS)
      {
         LOG(0, "Error at MPI size get !!\n");
      }

      if (mpi_rank == 0)
      {
         Parameters::printParams();
         printf("c file: %s\n", Parameters::getFilename());
      }

      // Diversification across all the distributed system, even if one node diversification works
      if (Parameters::getBoolParam("initshuffle"))
      {
         SolverFactory::initshuffleDiversification(solvers, mpi_rank, cpus);
      }
      SolverFactory::diversification(solvers);
      
      // Global Strategy initialization

      int maxClauseSize = Parameters::getIntParam("maxClauseSize", 50);
      if (Parameters::getBoolParam("one-sharer")) // same thread will access the globaldatabase
      {
         globalDatabase = new GlobalDatabase(nextId++, new ClauseDatabaseVector(maxClauseSize), new ClauseDatabaseVector(maxClauseSize));
      }
      else // at least two threads on globaldatabase
      {
         globalDatabase = new GlobalDatabase(nextId++, new ClauseDatabaseLockFree(maxClauseSize), new ClauseDatabaseLockFree(maxClauseSize));
      }
      switch (Parameters::getIntParam("gshr-strat", 1))
      {
      case 1:
         LOG(0, "GSTRAT>> AllGatherSharing\n");
         g_strat = new AllGatherSharing(nextId, globalDatabase);
         break;
      case 2:
         LOG(0, "GSTRAT>> MallobSharing\n");
         g_strat = new MallobSharing(nextId, globalDatabase);
         break;
      case 3:
         LOG(0, "GSTRAT>> RingSharing\n");
         g_strat = new RingSharing(nextId, globalDatabase);
         break;
      default:
         cout << "The choices for global sharing strategies are:\n\t1- AllGatherSharing\n\t2- MallobSharing(Tree)\n\t3- RingSharing\n"
              << endl;
         exit(1);
         break;
      }
      if (g_strat->initMpiVariables())
         sharingStrategies.push_back(g_strat);
      else
      {
         LOG(0, "Global strategy was not initialized correctly so dist will be reset to false and MPI will finalize.\n");
         dist = false;
         if (MPI_Finalize() != MPI_SUCCESS)
         {
            LOG(0, "Error at MPI finalize !!");
         }
      }
   }
   else
   {
      if (Parameters::getBoolParam("initshuffle"))
      {
         SolverFactory::initshuffleDiversification(solvers);
      }
      SolverFactory::diversification(solvers);

      Parameters::printParams();
      printf("c file: %s\n", Parameters::getFilename());
   }
#else
   if (Parameters::getBoolParam("initshuffle"))
   {
      SolverFactory::initshuffleDiversification(solvers);
   }
   SolverFactory::diversification(solvers);

   Parameters::printParams();
   printf("c file: %s\n", Parameters::getFilename());

   if (dist)
   {
      LOG(0, "Warning, you need compiled using the flag -DNDIST and you executed using -dist flag!\n");
      exit(1);
   }
#endif
sharersInit:
   // Init Sharing
   // 15 CDCL, 1 Reducer producers by Sharer
   std::vector<SharingEntity *> prod1;
   std::vector<SharingEntity *> prod2;
   std::vector<SharingEntity *> cons1;
   std::vector<SharingEntity *> cons2;
   std::vector<SharingEntity *> consCDCL;

#ifndef NDIST
   // Adding the global database as a producer and consumer to all possible sharers
   if (dist)
   {
      prod1.push_back(globalDatabase);
      cons1.push_back(globalDatabase);
      prod2.push_back(globalDatabase);
      cons2.push_back(globalDatabase);
   }
#endif
   if (shr_strat == 0)
   {
      std::random_device dev;
      std::mt19937 rng(dev());
      std::uniform_int_distribution<std::mt19937::result_type> distShr(1, 4);
      shr_strat = distShr(rng);
   }

   LocalSharingStrategy *l_strat;
   switch (shr_strat)
   {
   case 1:
      LOG(0, "LSTRAT>> HordeSatSharing(1Grp)\n");
      prod1.insert(prod1.end(), solvers.begin(), solvers.end());
      cons1.insert(cons1.end(), solvers.begin(), solvers.end());

      sharingStrategies.push_back(new HordeSatSharing(nextId, std::move(prod1), std::move(cons1)));
      ++nextId;
      break;
   case 2:
      if (cpus <= 2)
      {
         std::cerr << "Please select another sharing strategy other than 2 or 3 if you want to have " << cpus << " solvers." << std::endl;
         std::cerr << "If you used -dist option, the strategies may not work" << std::endl;
         exit(1);
      }
      LOG(0, "LSTRAT>> HordeStrSharing(2Grp)\n");
      reducers.push_back(SolverFactory::createReducerSolver());
      reducers.push_back(SolverFactory::createReducerSolver());

      prod1.insert(prod1.end(), solvers.begin(), solvers.begin() + nSolvers / 2);
      prod1.push_back(reducers[0]);
      prod2.insert(prod2.end(), solvers.begin() + nSolvers / 2, solvers.end());
      prod2.push_back(reducers[1]);
      cons1.insert(cons1.end(), solvers.begin(), solvers.end());
      cons1.push_back(reducers[0]);
      cons2.insert(cons2.end(), solvers.begin(), solvers.end());
      cons2.push_back(reducers[1]);

      sharingStrategies.push_back(new HordeStrSharing(nextId++, std::move(prod1), std::move(cons1), reducers[0]));
      sharingStrategies.push_back(new HordeStrSharing(nextId, std::move(prod2), std::move(cons2), reducers[1]));
      break;
   case 3:
      if (cpus <= 2)
      {
         std::cerr << "Please select another sharing strategy other than 2 or 3 if you want to have " << cpus << " solvers." << std::endl;
         std::cerr << "If you used -dist option, the strategies may not work" << std::endl;
         exit(1);
      }
      LOG(0, "LSTRAT>> HordeSatSharing(2Grp)\n");
      prod1.insert(prod1.end(), solvers.begin(), solvers.begin() + nSolvers / 2);
      prod1.push_back(solvers[solvers.size() - 1]);
      prod2.insert(prod2.end(), solvers.begin() + nSolvers / 2, solvers.end() - 1);

      cons1.insert(cons1.end(), solvers.begin(), solvers.begin() + nSolvers / 2);
      cons1.push_back(solvers[solvers.size() - 1]);
      cons2.insert(cons2.end(), solvers.begin() + nSolvers / 2, solvers.end() - 1);

      sharingStrategies.push_back(new HordeSatSharing(nextId++, std::move(prod1), std::move(cons1)));
      sharingStrategies.push_back(new HordeSatSharing(nextId++, std::move(prod1), std::move(cons1)));
      break;
   case 4:
      LOG(0, "LSTRAT>> HordeSatSharingAlt\n");
      prod1.insert(prod1.end(), solvers.begin(), solvers.end());
      cons1.insert(cons1.end(), solvers.begin(), solvers.end());
      // nSharers = 1;
      // sharers = new Sharer *[nSharers];
      // sharers[0] = new Sharer(nextId, new HordeSatSharingAlt(nextId, std::move(prod1), std::move(cons1)));
      sharingStrategies.push_back(new HordeSatSharingAlt(nextId, std::move(prod1), std::move(cons1)));
      break;
   case 5:
      LOG(0, "LSTRAT>> SimpleSharing\n");
      prod1.insert(prod1.end(), solvers.begin(), solvers.end());
      cons1.insert(cons1.end(), solvers.begin(), solvers.end());

      sharingStrategies.push_back(new SimpleSharing(nextId, std::move(prod1), std::move(cons1)));
      break;
   default:
      LOG(0, "use option: shr-strat=<INT>\t\t strategy for local sharing value in [1,5], 0 by default\n");
      break;
   }

   // load formula to all solvers
   int nReducers = reducers.size();
   pthread_t *ptr;
   ptr = new pthread_t[nSolvers + nReducers];

   // launch readers
   for (int i = 0; i < nSolvers; i++)
   {
      pthread_create(&ptr[i], NULL, readWorker, solvers[i]);
   }
   for (int i = nSolvers; i < nSolvers + nReducers; i++)
   {
      pthread_create(&ptr[i], NULL, readWorker, reducers[i]);
   }

   // join readers
   for (int i = 0; i < nSolvers; i++)
   {
      pthread_join(ptr[i], NULL);
   }
   for (int i = nSolvers; i < nSolvers + nReducers; i++)
   {
      pthread_join(ptr[i], NULL);
   }
   delete[] ptr;

   LOG(1, "c finish solver read, use %.2lf seconds\n", getRelativeTime());

   // Init timeout detection before starting the solvers and sharers
   pthread_mutex_init(&mutexGlobalEnd, NULL);
   pthread_cond_init(&condGlobalEnd, NULL);

   // Init the management of clauses
   ClauseManager::initClauseManager();

   pthread_mutex_lock(&mutexGlobalEnd); // to make sure that the broadcast is done when main has done its wait
   // Init working
   working = new Portfolio();
   for (size_t i = 0; i < nSolvers; i++)
   {
      working->addSlave(new SequentialWorker(solvers[i]));
   }

   for (size_t i = 0; i < nReducers; i++)
   {
      working->addSlave(new SequentialWorker(reducers[i]));
   }

   LOG(3, "%d workers added to Portfolio!\n", nSolvers + nReducers);

   // Launch working
   std::vector<int> cube;
   working->solve(cube);

   if (Parameters::getBoolParam("one-sharer"))
   {
      sharers.push_back(new SharerMulti(-1, sharingStrategies));
   }
   else
   {
      for (auto strat : sharingStrategies)
      {
         sharers.push_back(new Sharer(strat->getId(), strat));
      }
   }

   int timeout = Parameters::getIntParam("t", 5000); // if <0 will loop till a solution is found
   if (timeout > 0)
   {
      // Wait until end or timeout
      timespec timeoutSpec = {timeout, 0};
      timespec timespecCond;
      int wakeupRet = 0;

      // while (globalEnding == false)
      // {
      // sleep(1);
      while ((int)getRelativeTime() < timeout && globalEnding == false) // to manage the spurious wake ups
      {
         timeoutSpec.tv_sec = (timeout - (int)getRelativeTime()); // count only seconds
         getTimeToWait(&timeoutSpec, &timespecCond);
         wakeupRet = pthread_cond_timedwait(&condGlobalEnd, &mutexGlobalEnd, &timespecCond);
         LOG(3, "main wakeupRet = %d , globalEnding = %d \n", wakeupRet, globalEnding.load());
      }

      if (wakeupRet == ETIMEDOUT && !globalEnding) // if timeout set globalEnding otherwise a solver woke me up
      {
         globalEnding = true;
         finalResult = TIMEOUT;
         working->setInterrupt();
      }
      // in case the sharers have done their wait too late
      pthread_cond_broadcast(&condGlobalEnd);

      pthread_mutex_unlock(&mutexGlobalEnd);
   }
   else
   {
      // no timeout waiting
   }
   // }

   // Join all sharer threads
   for (auto &sharer : sharers)
   {
      sharer->join();
   }

   // Delete and print stat in another loop to not have mixed prints
   for (auto &sharer : sharers)
   {
      sharer->printStats();
      delete sharer;
   }

   // Delete Sharing Strategies
   for (auto &strat : sharingStrategies)
   {
      delete strat;
   }

   // Print solver stats
   // SolverFactory::printStats(solvers);

   // Delete working strategy
   delete working;

   // Delete shared clauses
   ClauseManager::joinClauseManager();

#ifndef NDIST
   // Delete global sharer
   if (dist)
   {
      // g_sharer->printStats();
      // delete g_sharer;
      globalDatabase->release();

      if (mpi_rank == 0)
         cout << "c Resolution time: " << getRelativeTime() << "s" << endl;
      if (MPI_Finalize() != MPI_SUCCESS)
      {
         LOG(0, "Error at MPI finalize !!");
      }
   }
   else
   {
      cout << "c Resolution time: " << getRelativeTime() << "s" << endl;
   }
#else
   cout << "c Resolution time: " << getRelativeTime() << "s" << endl;
#endif

   // Print the result and the model if SAT
   // cout << "c Resolution time: " << getRelativeTime() << "s" << endl;
   // cout << "t Time spent: " << getRelativeTime() << " s" << endl;

   cout << endl;

   if (finalResult == SAT)
   {
      cout << "s SATISFIABLE" << endl;
      if (Parameters::getBoolParam("no-model") == false && finalModel.size() != 0) // In case this process received SAT but not the finalModel
      {
         if (Parameters::getBoolParam("simp"))
         {
            for (int i = 1; i <= S->orivars; i++)
               if (S->mapto[i])
                  S->mapval[i] = (finalModel[abs(S->mapto[i]) - 1] > 0 ? 1 : -1) * (S->mapto[i] > 0 ? 1 : -1);
            S->print_complete_model();
            finalModel.clear();
            for (int i = 1; i <= S->orivars; i++)
            {
               finalModel.push_back(i * S->mapval[i]);
            }
            printModel(finalModel);
         }
         else
            printModel(finalModel);
      }
   }
   else if (finalResult == UNSAT)
   {
      cout << "s UNSATISFIABLE" << endl;
   }
   else if (finalResult == TIMEOUT)
   {
      cout << "s TIMEOUT" << endl;
   }
   else
   {
      cout << "s UNKNOWN" << endl;
   }

   return 0;
}

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
#include "HordeSatSharingAlt.h"

#include "clauses/ClauseManager.h"
#include "solvers/SolverFactory.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "painless.h"

HordeSatSharingAlt::HordeSatSharingAlt(int id, std::vector<SharingEntity *> &producers, std::vector<SharingEntity *> &consumers) : LocalSharingStrategy(id, producers, consumers)
{
   // shr-lit * solver_nbr
   this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
   this->round = 0;
   this->initPhase = true;
   // number of round corresponding to 5% of the 5000s timeout
   int sleepTime = Parameters::getIntParam("shr-sleep", 500000);
   if (sleepTime)
      this->roundBeforeIncrease = 250000000 / sleepTime;
   else
   {
      LOG(1, "Warning: sleepTime of sharing strategy is set to: %d", sleepTime);
      this->roundBeforeIncrease = 250000000 / 500000;
   }
   // Init database
   this->database = new ClauseDatabaseVector();

   if (Parameters::getBoolParam("dup"))
   {
      this->filter = new BloomFilter();
   }
}

HordeSatSharingAlt::HordeSatSharingAlt(int id, std::vector<SharingEntity *> &&producers, std::vector<SharingEntity *> &&consumers) : LocalSharingStrategy(id, producers, consumers)
{
   // shr-lit * solver_nbr
   this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
   this->round = 0;
   this->initPhase = true;
   // number of round corresponding to 5% of the 5000s timeout
   int sleepTime = Parameters::getIntParam("shr-sleep", 500000);
   if (sleepTime)
      this->roundBeforeIncrease = 250000000 / sleepTime;
   else
   {
      LOG(1, "Warning: sleepTime of sharing strategy is set to: %d", sleepTime);
      this->roundBeforeIncrease = 250000000 / 500000;
   }

   // Init database
   this->database = new ClauseDatabaseVector();

   if (Parameters::getBoolParam("dup"))
   {
      this->filter = new BloomFilter();
   }
}

HordeSatSharingAlt::~HordeSatSharingAlt()
{
   delete database;
   if (Parameters::getBoolParam("dup"))
   {
      delete filter;
   }
}

bool HordeSatSharingAlt::doSharing()
{
   if (globalEnding)
      return true;
   // 1- Fill the database using all the producers
   for (auto producer : producers)
   {
      unfiltered.clear();
      filtered.clear();

      if (Parameters::getBoolParam("dup"))
      {
         producer->exportClauses(unfiltered);
         // ref = 1
         for (ClauseExchange *c : unfiltered)
         {
            if (!filter->contains_or_insert(c->lits))
            {
               filtered.push_back(c);
            }
         }
         stats.receivedClauses += unfiltered.size();
         stats.receivedDuplicas += (unfiltered.size() - filtered.size());
      }
      else
      {
         producer->exportClauses(filtered);
         stats.receivedClauses += filtered.size();
      }

      // if solver checks the usage percentage else nothing fancy to do
      producer->accept(this);

      for (auto cls : filtered)
      {
         this->database->addClause(cls); // if not added it is released : ref = 0
      }
   }

   unfiltered.clear();
   filtered.clear();
   // 2- Get selection
   // consumer receives the same amount as in the original
   this->database->giveSelection(filtered, literalPerRound * producers.size());

   stats.sharedClauses += filtered.size();

   // 3-Send the best clauses (all producers included) to all consumers
   for (auto consumer : consumers)
   {
      for (auto cls : filtered)
      {
         if (cls->from != consumer->id)
         {
            consumer->importClause(cls);
         }
      }
   }

   // release the clauses of this context
   for (auto cls : filtered)
   {
      ClauseManager::releaseClause(cls);
   }

   LOG(1, "[HordeSatAlt %d] received cls %ld, shared cls %ld\n", idSharer, stats.receivedClauses, stats.sharedClauses);

   if (globalEnding)
      return true;
   return false;
}

void HordeSatSharingAlt::visit(SolverInterface *solver)
{
   LOG(3, "[HordeSatAlt %d] Visiting the solver %d\n", idSharer, solver->id);

   // Here I check the number of produced clauses before selecting them (maybe will not cause solvers to export potentially useless clauses)
   int usedPercent;
   usedPercent = (100 * SharingStrategy::getLiteralsCount(filtered)) / literalPerRound;
   if (usedPercent < 75 && !this->initPhase)
   {
      solver->increaseClauseProduction();
      LOG(3, "[HordeAlt %d] production increase for solver %d.\n", this->idSharer, solver->id);
   }
   else if (usedPercent > 98)
   {
      solver->decreaseClauseProduction();
      LOG(3, "[HordeAlt %d] production decrease for solver %d.\n", this->idSharer, solver->id);
   }
   if (round >= this->roundBeforeIncrease)
   {
      this->initPhase = false;
   }
}

void HordeSatSharingAlt::visit(SharingEntity *sh_entity)
{
   LOG(3, "[HordeSatAlt %d] Visiting the sh_entity %d\n", idSharer, sh_entity->id);
}

#ifndef NDIST
void HordeSatSharingAlt::visit(GlobalDatabase *g_base)
{
   LOG(3, "[HordeSatAlt %d] Visiting the global database %d\n", idSharer, g_base->id);

   LOG(1, "[HordeSatAlt %d] Added %d clauses imported from another process\n", idSharer, filtered.size());
}
#endif
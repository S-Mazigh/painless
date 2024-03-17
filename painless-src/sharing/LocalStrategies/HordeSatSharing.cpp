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
#include "HordeSatSharing.h"
#include "clauses/ClauseManager.h"
#include "solvers/SolverFactory.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/BloomFilter.h"
#include "painless.h"

HordeSatSharing::HordeSatSharing(int id, std::vector<SharingEntity *> &producers, std::vector<SharingEntity *> &consumers) : LocalSharingStrategy(id, producers, consumers)
{
   this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
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
   if (Parameters::getBoolParam("dup"))
   {
      this->filter = new BloomFilter();
   }
}

HordeSatSharing::HordeSatSharing(int id, std::vector<SharingEntity *> &&producers, std::vector<SharingEntity *> &&consumers) : LocalSharingStrategy(id, producers, consumers)
{
   this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
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
   if (Parameters::getBoolParam("dup"))
   {
      this->filter = new BloomFilter();
   }
}

HordeSatSharing::~HordeSatSharing()
{
   for (auto pair : this->databases)
   {
      delete pair.second;
   }
   if (Parameters::getBoolParam("dup"))
   {
      delete filter;
   }
}

bool HordeSatSharing::doSharing()
{
   if (globalEnding)
      return true;
   for (auto producer : producers)
   {
      if (!this->databases.count(producer->id))
      {
         this->databases[producer->id] = new ClauseDatabaseVector();
      }
      unfiltered.clear();
      filtered.clear();

      if (Parameters::getBoolParam("dup"))
      {
         producer->exportClauses(unfiltered);
         // ref = 1
         for (ClauseExchange *c : unfiltered)
         {
            uint8_t count = filter->test_and_insert(c->checksum, 12);
            if (count == 1 || (count == 6 && c->lbd > 6) || (count == 11 && c->lbd > 2))
            {
               filtered.push_back(c);
            }
            if (count == 6 && c->lbd > 6)
            { // to promote (tiers2) and share
               c->lbd = 6;
               ++stats.promotionTiers2;
               ++stats.receivedDuplicas;
            }
            else if (count == 6)
            {
               ++stats.alreadyTiers2;
            }
            else if (count == 11 && c->lbd > 2)
            { // to promote (core) and share
               c->lbd = 2;
               ++stats.promotionCore;
               ++stats.receivedDuplicas;
            }
            else if (count == 11)
            {
               ++stats.alreadyCore;
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
      for (auto cls : filtered)
      {
         this->databases[producer->id]->addClause(cls); // if not added it is released : ref = 0
      }
      unfiltered.clear();
      filtered.clear();

      // get selection and checks the used percentage if producer is a solver
      producer->accept(this);

      stats.sharedClauses += filtered.size();

      for (auto consumer : consumers)
      {
         if (producer->id != consumer->id)
         {
            consumer->importClauses(filtered); // if imported ref++
         }
      }
      // release the selection
      for (auto cls : filtered)
      {
         ClauseManager::releaseClause(cls);
      }
   }

   round++;

   LOG(1, "[HordeSat %d] received cls %ld, shared cls %ld\n", idSharer, stats.receivedClauses, stats.sharedClauses);
   if (globalEnding)
      return true;
   return false;
}

void HordeSatSharing::visit(SolverInterface *solver)
{
   LOG(3, "[HordeSat %d] Visiting the solver %d\n", idSharer, solver->id);
   int used, usedPercent, selectCount;

   used = this->databases[solver->id]->giveSelection(filtered, literalPerRound, &selectCount);
   usedPercent = (100 * used) / literalPerRound;

   stats.sharedClauses += filtered.size();

   if (usedPercent < 75)
   {
      solver->increaseClauseProduction();
      LOG(3, "[HordeSat %d] production increase for solver %d.\n", idSharer, solver->id);
   }
   else if (usedPercent > 98)
   {
      solver->decreaseClauseProduction();
      LOG(3, "[HordeSat %d] production decrease for solver %d.\n", idSharer, solver->id);
   }

   if (selectCount > 0)
   {
      LOG(3, "[HordeSat %d] filled %d%% of its buffer %.2f\n", idSharer, usedPercent, used / (float)selectCount);

      this->initPhase = false;
   }
}

void HordeSatSharing::visit(SharingEntity *sh_entity)
{
   LOG(3, "[HordeSat %d] Visiting the sharing entity %d\n", idSharer, sh_entity->id);

   this->databases[sh_entity->id]->giveSelection(filtered, literalPerRound);
}

#ifndef NDIST

void HordeSatSharing::visit(GlobalDatabase *g_base)
{
   LOG(3, "[HordeSat %d] Visiting the global database %d\n", idSharer, g_base->id);

   this->databases[g_base->id]->giveSelection(filtered, literalPerRound);

   LOG(1, "[HordeSat %d] Added %d clauses imported from another process\n", idSharer, filtered.size());
}
#endif
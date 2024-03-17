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

#pragma once

#include "LocalSharingStrategy.h"
#include "clauses/ClauseDatabaseVector.h"
#include "utils/BloomFilter.h"

#include <unordered_map>
#include <vector>

/// @brief This strategy is a hordesat like sharing strategy.
/// \ingroup local_sharing_strat
class HordeSatSharing : public LocalSharingStrategy
{
public:
   /// Constructors
   HordeSatSharing(int id, std::vector<SharingEntity *> &producers, std::vector<SharingEntity *> &consumers);
   HordeSatSharing(int id, std::vector<SharingEntity *> &&producers, std::vector<SharingEntity *> &&consumers);

   /// Destructor.
   ~HordeSatSharing();

   /// This method shared clauses from the producers to the consumers.
   bool doSharing() override;

   /// @brief  Each producer fill its database, then a selection is done which is sent to all the consumers.
   /// @param solver the solver to interact with (visit)
   void visit(SolverInterface *solver) override;

   /// @brief The default behavior of any sharingEntity if this strategy
   /// @param sh_entity the sharing entityity to interact with (visit)
   void visit(SharingEntity *sh_entity) override;
#ifndef NDIST
   /// @brief The  behavior of with global database. Import to toSend, export from Received
   /// @param g_base the global database to interact with (visit)
   void visit(GlobalDatabase *g_base) override;
#endif
   /// @brief Specific behavior to have with a reducer (a special type of solvers). Behavior is the same as with generic solver in HordesatSharing
   /// @param reducer the reducer to interact with (visit)
   // void visit(Reducer *reducer) override;

protected:
   /// Number of shared literals per round.
   int literalPerRound;

   /// Are we in init phase.
   bool initPhase;

   /// Number of round before forcing an increase in production
   int roundBeforeIncrease;

   /// @brief Round Number
   int round;

   /// Databse used to store the clauses.
   unordered_map<int, ClauseDatabaseVector *> databases;
   unordered_map<int, ClauseDatabaseVector *> unfiltered_databases;

   /// Used to manipulate clauses.
   std::vector<ClauseExchange *> filtered;
   std::vector<ClauseExchange *> unfiltered;

   /// @brief Filter used if -dup mode enabled
   BloomFilter *filter = nullptr;
};

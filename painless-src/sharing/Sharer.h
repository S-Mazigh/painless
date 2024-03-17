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

#include "sharing/SharingStrategy.h"
#include "sharing/SharingEntity.h"
#include "utils/Threading.h"

static void *mainThrSharing(void *arg);

/// \defgroup sharing Sharing Related Classes
/// \ingroup sharing

/// @brief A sharer is a thread responsible to share clauses between solvers.
class Sharer
{
public:
   /// Constructors.
   Sharer(int id_, SharingStrategy *sharingStrategy_);

   Sharer(int id_);

   /// Destructor.
   virtual ~Sharer();

   /// Add a sharingEntity to the producers.
   void addProducer(SharingEntity *sharingEntity);

   /// Add a sharingEntity to the consumers.
   void addConsumer(SharingEntity *sharingEntity);

   /// Remove a sharingEntity from the producers.
   void removeProducer(SharingEntity *sharingEntity);

   /// Remove a sharingEntity from the consumers.
   void removeConsumer(SharingEntity *sharingEntity);

   /// Print sharing statistics.
   virtual void printStats();

   /// @brief To join the thread of this sharer object
   inline void join()
   {
      LOG(2, "Sharer %d joined\n", id);
      sharer->join();
   }

protected:
   friend void *mainThrSharing(void *);

   /// Id of the sharer.
   int id;

   /// Mutex used to add producers and consumers.
   Mutex addLock;

   /// Mutex used to add producers and consumers.
   Mutex removeLock;

   /// Vector of solvers or GStrats to add to the producers.
   std::vector<SharingEntity *> addProducers;

   /// Vector of solvers or GStrats to add to the consumers.
   std::vector<SharingEntity *> addConsumers;

   /// Vector of solvers or GStrats to remove from the producers.
   std::vector<SharingEntity *> removeProducers;

   /// Vector of solvers or GStrats to remove from the consumers.
   std::vector<SharingEntity *> removeConsumers;

   /// Pointer to the thread in chrage of sharing.
   Thread *sharer;

private:
   /// Strategy used to shared clauses.
   SharingStrategy *sharingStrategy;
};

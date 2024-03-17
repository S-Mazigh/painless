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

#include "painless.h"
#include "sharing/Sharer.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/System.h"

#include <algorithm>
#include <unistd.h>

/// Function exectuted by each sharer.
/// This is main of sharer threads.
/// @param  arg contains a pointer to the associated class
/// @return return NULL if the thread exit correctly
static void *mainThrSharing(void *arg)
{
   Sharer *shr = (Sharer *)arg;
   bool can_break = false;
   int round = 0;
   int wakeupRet = 0;
   int sleepTime = shr->sharingStrategy->getSleepingTime();
   timespec sleepTimeSpec;
   getTimeSpecMicro(sleepTime, &sleepTimeSpec);
   timespec timespecCond;
   usleep(Parameters::getIntParam("init-sleep", 0));
   LOG(1, "Sharer #%d will start now, sleeptime: %d usec\n", shr->id, sleepTime);

   // SharingStatistics stats;

   while (!can_break)
   {
      // Sleep
      // usleep(sleepTime);
      // no waiting if it is the end
      if (!globalEnding)
      {

         // wait for sleeptime time then wakeup (spurious ok)
         // if signaled the globalending will be managed by the strategy
         pthread_mutex_lock(&mutexGlobalEnd);
         getTimeToWait(&sleepTimeSpec, &timespecCond);
         wakeupRet = pthread_cond_timedwait(&condGlobalEnd, &mutexGlobalEnd, &timespecCond);
         pthread_mutex_unlock(&mutexGlobalEnd);
         LOG(3, "sharer wakeupRet = %d , globalEnding = %d \n", wakeupRet, globalEnding.load());
         // usleep(sleepTime);
      }
      if (wakeupRet != ETIMEDOUT && wakeupRet != 0)
      {
         LOG(3, "Error %d on pthread_cond_wait in sharer, decided to do a simple sleep!", wakeupRet);
         usleep(sleepTime);
      }
      round++; // New round

      // Add new solvers
      // -------------------------
      /*shr->addLock.lock();

      shr->producers.insert(shr->producers.end(), shr->addProducers.begin(),
                            shr->addProducers.end());
      shr->addProducers.clear();

      shr->consumers.insert(shr->consumers.end(), shr->addConsumers.begin(),
                            shr->addConsumers.end());
      shr->addConsumers.clear();

      shr->addLock.unlock();*/

      // Sharing phase
      LOG(1, "Sharer %d will enter round  %d:\n", shr->id, round);
      can_break = shr->sharingStrategy->doSharing();
      // Remove solvers
      // -------------------------
      /*shr->removeLock.lock();

      for (size_t i = 0; i < shr->removeProducers.size(); i++) {
         shr->producers.erase(remove(shr->producers.begin(),
                                     shr->producers.end(),
                                     shr->removeProducers[i]),
                              shr->producers.end());
         shr->removeProducers[i]->release();
      }
      shr->removeProducers.clear();

      for (size_t i = 0; i < shr->removeConsumers.size(); i++) {
         shr->consumers.erase(remove(shr->consumers.begin(),
                                     shr->consumers.end(),
                                     shr->removeConsumers[i]),
                              shr->consumers.end());
         shr->removeConsumers[i]->release();
      }
      shr->removeConsumers.clear();

      shr->removeLock.unlock();*/

      if (can_break)
      {
         LOG(3, "Strategy '%s' ended with globalEnding = %d\n", typeid(*(shr->sharingStrategy)).name(), globalEnding.load());
         break; // Need to stop
      }
   }
   // in case it is the sharing strategy that detects first the ending.
   // IMPORTANT: a strategy can decide to stop the sharer at any time it wants other than when global_ending = true. Thus the if before the broadcast on the conditional variable.
   if (globalEnding)
   {
      pthread_mutex_lock(&mutexGlobalEnd);
      pthread_cond_broadcast(&condGlobalEnd);
      pthread_mutex_unlock(&mutexGlobalEnd);
   }
   return NULL;
}

Sharer::Sharer(int id, SharingStrategy *sharingStrategy)
{
   this->id = id;
   this->sharingStrategy = sharingStrategy;

   sharer = new Thread(mainThrSharing, this);
}

Sharer::Sharer(int id_) : id(id_)
{
   sharingStrategy = nullptr;
}

Sharer::~Sharer()
{
   this->join();
   delete sharer;

   removeLock.lock();

   for (int i = 0; i < removeProducers.size(); i++)
   {
      removeProducers[i]->release();
   }

   for (size_t i = 0; i < removeConsumers.size(); i++)
   {
      removeConsumers[i]->release();
   }

   removeLock.unlock();
}

void Sharer::addProducer(SharingEntity *sharingEntity)
{
   sharingEntity->increase();

   addLock.lock();
   addProducers.push_back(sharingEntity);
   addLock.unlock();
}

void Sharer::addConsumer(SharingEntity *sharingEntity)
{
   sharingEntity->increase();

   addLock.lock();
   addConsumers.push_back(sharingEntity);
   addLock.unlock();
}

void Sharer::removeProducer(SharingEntity *sharingEntity)
{
   removeLock.lock();
   removeProducers.push_back(sharingEntity);
   removeLock.unlock();
}

void Sharer::removeConsumer(SharingEntity *sharingEntity)
{
   removeLock.lock();
   removeConsumers.push_back(sharingEntity);
   removeLock.unlock();
}

void Sharer::printStats()
{

   this->sharingStrategy->printStats();
}
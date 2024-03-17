#include "SharerMulti.h"
#include "painless.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/System.h"

#include <algorithm>
#include <unistd.h>

/// Function exectuted by each SharerMulti.
/// This is main of SharerMulti threads.
/// @param  arg contains a pointer to the associated class
/// @return return NULL if the thread exit correctly
static void *mainThrSharingMulti(void *arg)
{
    SharerMulti *shr = (SharerMulti *)arg;
    int round = 0;
    int nbStrats = shr->sharingStrategies.size();
    int lastStrategy = -1;
    int sleepTime = Parameters::getIntParam("shr-sleep", 500000) / nbStrats; // TODO : to be replaced
    int wakeupRet = 0;
    timespec sleepTimeSpec = {0, sleepTime * 1000};
    timespec timespecCond;
    sleep(Parameters::getIntParam("init-sleep", 0));
    LOG(0, "SharerMulti #%d will start now, sleeptime: %d usec\n", shr->id, sleepTime);

    bool can_break = false;

    while (!can_break)
    {
        // Sleep
        // usleep(sleepTime);
        pthread_mutex_lock(&mutexGlobalEnd);
        getTimeToWait(&sleepTimeSpec, &timespecCond);
        wakeupRet = pthread_cond_timedwait(&condGlobalEnd, &mutexGlobalEnd, &timespecCond);
        pthread_mutex_unlock(&mutexGlobalEnd);

        lastStrategy = round % nbStrats;

        round++; // New round

        // Sharing phase
        LOG(1, "SharerMulti %d will enter round  %d using strategy %d:\n", shr->id, round, lastStrategy);
        can_break = shr->sharingStrategies[lastStrategy]->doSharing();
    }

    // Removed strategy that ended
    LOG(3, "SharerMulti %d strategy %d ended\n", shr->id, lastStrategy);
    shr->sharingStrategies.erase(shr->sharingStrategies.begin() + lastStrategy);
    nbStrats--;

    LOG(3, "SharerMulti %d has %d remaining strategies.\n", shr->id, nbStrats);

    // Launch a final doSharing to make the other strategies finalize correctly (removed from the previous while(1) to lessen ifs)
    for (int i = 0; i < nbStrats; i++)
    {
        LOG(3, "SharerMulti %d will end strategy %d\n", shr->id, i);
        if (!shr->sharingStrategies[i]->doSharing())
        {
            LOG(0, "Panic, strategy %d didn't detect ending!", i);
        }
    }
    // IMPORTANT: a strategy can decide to stop the sharer at any time it wants other than when global_ending = true. Thus the if before the broadcast on the conditional variable.
   if (globalEnding)
   {
      pthread_mutex_lock(&mutexGlobalEnd);
      pthread_cond_broadcast(&condGlobalEnd);
      pthread_mutex_unlock(&mutexGlobalEnd);
   }
    return NULL;
}

SharerMulti::SharerMulti(int id_, std::vector<SharingStrategy *> &sharingStrategies_) : Sharer(id_), sharingStrategies(sharingStrategies_)
{
    sharer = new Thread(mainThrSharingMulti, this);
}

SharerMulti::~SharerMulti()
{
    LOG(1, "SharerMulti %d will end\n", this->id);
    // this->join(); // done in sharer destructor
    // delete sharer;
}

void SharerMulti::printStats()
{
    for (auto sharingStrategy : sharingStrategies)
    {
        std::cout << "c Strategy '" << typeid(*sharingStrategy).name() << "' : " << std::endl;
        sharingStrategy->printStats();
    }
}
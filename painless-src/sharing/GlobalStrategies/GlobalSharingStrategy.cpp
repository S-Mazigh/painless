#ifndef NDIST
#include "GlobalSharingStrategy.h"
#include "painless.h"
#include "utils/Parameters.h"
#include "clauses/ClauseManager.h"
#include <list>

GlobalSharingStrategy::GlobalSharingStrategy(int id, GlobalDatabase *g_base) : SharingStrategy(id)
{
    this->globalDatabase = g_base;
    g_base->increase();
}

GlobalSharingStrategy::~GlobalSharingStrategy()
{
    globalDatabase->release();
}

GlobalDatabase *GlobalSharingStrategy::getSharingEntity()
{
    return globalDatabase;
}

void GlobalSharingStrategy::printStats()
{
    cout << "\n=====\nc Global Sharer[" << mpi_rank << "-" << idSharer << "]:"
         << "receivedCls " << gstats.receivedClauses
         << ",sharedCls " << gstats.sharedClauses
         << ",receivedDuplicas " << gstats.receivedDuplicas
         << ",sharedDuplicasAvoided " << gstats.sharedDuplicasAvoided
         << ",messagesSent " << gstats.messagesSent
         << "\n=====\n"
         << endl;
}

ulong GlobalSharingStrategy::getSleepingTime()
{
    return Parameters::getIntParam("shr-sleep", 500000) * 3;
}
#endif
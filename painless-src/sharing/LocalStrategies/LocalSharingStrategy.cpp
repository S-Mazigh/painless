#include "LocalSharingStrategy.h"
#include "painless.h"

LocalSharingStrategy::LocalSharingStrategy(int id, std::vector<SharingEntity *> &producers, std::vector<SharingEntity *> &consumers) : SharingStrategy(id)
{
    this->producers = producers;
    this->consumers = consumers;
    this->idSharer = id;

    for (size_t i = 0; i < producers.size(); i++)
    {
        producers[i]->increase();
    }

    for (size_t i = 0; i < consumers.size(); i++)
    {
        consumers[i]->increase();
    }
}

LocalSharingStrategy::LocalSharingStrategy(int id, std::vector<SharingEntity *> &&producers, std::vector<SharingEntity *> &&consumers) : SharingStrategy(id)
{
    this->producers = producers;
    this->consumers = consumers;
    this->idSharer = id;

    for (size_t i = 0; i < producers.size(); i++)
    {
        producers[i]->increase();
    }

    for (size_t i = 0; i < consumers.size(); i++)
    {
        consumers[i]->increase();
    }
}

void LocalSharingStrategy::printStats()
{
    cout << "\n=====\nc Local Sharer[" << mpi_rank << "-" << idSharer << "]:"
         << "receivedCls " << stats.receivedClauses
         << ",sharedCls " << stats.sharedClauses
         << ",receivedDuplicas " << stats.receivedDuplicas
         << ",promotionTiers2 " << stats.promotionTiers2
         << ",promotionsCore " << stats.promotionCore
         << ",alreadyTiers2 " << stats.alreadyTiers2
         << ",alreadyCore " << stats.alreadyCore
         << "\n=====\n"
         << endl;
}

LocalSharingStrategy::~LocalSharingStrategy()
{
    for (size_t i = 0; i < producers.size(); i++)
    {
        producers[i]->release();
    }

    for (size_t i = 0; i < consumers.size(); i++)
    {
        consumers[i]->release();
    }
}
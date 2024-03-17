#ifndef NDIST

#include "sharing/GlobalDatabase.h"
#include "utils/Parameters.h"
#include "painless.h"
#include <numeric>

GlobalDatabase::GlobalDatabase(int entity_id, ClauseDatabase *firstDB, ClauseDatabase *secondDB) : SharingEntity(entity_id), clausesToSend(firstDB), clausesReceived(secondDB)
{
    this->nRefs = 1;
}

GlobalDatabase::~GlobalDatabase()
{
    printf("c == Global Database Statistics ==\n \
    c\t[GDatabase %d] Final ToSend Database Size: %d Final Received Database Size: %d\n",
           mpi_rank, this->clausesToSend->getSize(), this->clausesReceived->getSize());

    printf("c \t[GDatabase %d] Sizes of clausesToSend : ", mpi_rank);
    this->clausesToSend->printTotalSizes();

    printf("c \t[GDatabase %d] Sizes of clausesReceived : ", mpi_rank);
    this->clausesReceived->printTotalSizes();

    std::cout << std::endl;

    delete clausesReceived;
    delete clausesToSend;
}

//---------------------------------------------------------
//             Local Import/Export Interface
//---------------------------------------------------------

void GlobalDatabase::importClauses(const std::vector<ClauseExchange *> &v_cls)
{
    for (auto cls : v_cls)
    {
        importClause(cls);
    }
}

void GlobalDatabase::exportClauses(std::vector<ClauseExchange *> &v_cls)
{
    clausesReceived->getClauses(v_cls);
}

void GlobalDatabase::exportClauses(std::vector<ClauseExchange *> &v_cls, uint totalSize)
{
    clausesReceived->giveSelection(v_cls, totalSize);
}

//---------------------------------------------------------
//           Useful functions for globalStrategy
//---------------------------------------------------------

void GlobalDatabase::addReceivedClauses(std::vector<ClauseExchange *> &v_cls)
{
    for (auto cls : v_cls)
    {
        addReceivedClause(cls);
    }
}

void GlobalDatabase::getClausesToSend(std::vector<ClauseExchange *> &v_cls)
{
    clausesToSend->getClauses(v_cls);
}

void GlobalDatabase::getClausesToSend(std::vector<ClauseExchange *> &v_cls, uint literals_count)
{
    clausesToSend->giveSelection(v_cls, literals_count);
}
#endif

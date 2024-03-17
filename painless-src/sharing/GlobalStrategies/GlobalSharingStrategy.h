#ifndef NDIST
#pragma once

#include "sharing/SharingStatistics.h"
#include "sharing/SharingStrategy.h"
#include "utils/BloomFilter.h"
#include "sharing/GlobalDatabase.h"

#include <mpi.h>

/// \defgroup global_sharing Global Sharing Strategies
/// \ingroup sharing_strat
/// \ingroup global_sharing
/// @brief The interface for all the different inter mpi processes sharing strategies
class GlobalSharingStrategy : public SharingStrategy
{

public:
    /// @brief Constructors
    /// @param id the id the sharer using this strategy
    GlobalSharingStrategy(int id, GlobalDatabase *g_base);

    ///  Destructor
    virtual ~GlobalSharingStrategy();

    /// @brief A function used to get the sleeping time of a sharer
    /// @return number of millisecond to sleep
    ulong getSleepingTime() override;

    /// @brief Function called to print the stats of the strategy
    void printStats() override;

    /// @brief Post constructor initialization done after MPI_Init()
    /// @return true if the initialization was done correctly, false otherwise
    virtual bool initMpiVariables() = 0;

    /// @brief To get the sharing entity of this strategy so it can be added to other sharing strategies
    /// @return a pointer to the GlobalDatabase of this GlobalSharingStrategy
    GlobalDatabase *getSharingEntity();

protected:
    /// @brief GlobalDatabase of the globalSharingStrategy
    GlobalDatabase *globalDatabase;

    /// Sharing statistics.
    GlobalSharingStatistics gstats;
};

#endif
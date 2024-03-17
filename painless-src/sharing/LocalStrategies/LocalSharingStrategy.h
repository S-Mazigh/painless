#pragma once

#include "sharing/SharingStrategy.h"
#include "sharing/SharingStatistics.h"
#include "sharing/SharingEntityVisitor.h"

// To include the real types
#include "solvers/SolverInterface.hpp"
#include "sharing/GlobalDatabase.h"
#include "sharing/SharingEntity.h"

#include <vector>

/**
 * \defgroup local_sharing_strat Local Sharing Strategies
 * \ingroup sharing_strat
 * \ingroup local_sharing_strat
 * @brief The interface for all the different local sharing strategies.
 *
 * The class LocalSharingStrategy follows the SharingEntityVisitor design pattern on \ref SharingEntity to implement the different
 * behavior they can have in a specific sharing strategy.
 */
class LocalSharingStrategy : public SharingEntityVisitor, public SharingStrategy
{
public:
    // constructor
    LocalSharingStrategy(int id, std::vector<SharingEntity *> &producers, std::vector<SharingEntity *> &consumers);
    LocalSharingStrategy(int id, std::vector<SharingEntity *> &&producers, std::vector<SharingEntity *> &&consumers);

    void printStats() override;

    virtual ~LocalSharingStrategy();

protected:
    /// @brief The vector holding the references to the producers
    std::vector<SharingEntity *> producers;

    /// @brief A vector hodling the references to the consumers
    std::vector<SharingEntity *> consumers;

    /// Sharing statistics.
    SharingStatistics stats;
};
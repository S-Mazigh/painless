#pragma once

#include "utils/Parameters.h"
#include "working/WorkingStrategy.h"

#include "preprocessors/PRS-Preprocessors/preprocess.hpp"

#include "sharing/Sharer.h"
#include "sharing/GlobalDatabase.h"
#include "sharing/LocalStrategies/LocalSharingStrategy.h"
#include "sharing/GlobalStrategies/GlobalSharingStrategy.h"
#include "preprocessors/StructuredBva.hpp"

#include <mutex>
#include <condition_variable>

class PortfolioSimple : public WorkingStrategy
{
public:
   PortfolioSimple();

   ~PortfolioSimple();

   void solve(const std::vector<int> &cube);

   void join(WorkingStrategy *strat, SatResult res,
             const std::vector<int> &model);

   void setInterrupt();

   void unsetInterrupt();

   void waitInterrupt();

protected:
   std::atomic<bool> strategyEnding;

   // Sharing
   //--------

   std::vector<std::shared_ptr<LocalSharingStrategy>> localStrategies;
   std::vector<std::shared_ptr<GlobalSharingStrategy>> globalStrategies;
   std::shared_ptr<GlobalDatabase> globalDatabase;
   std::vector<std::unique_ptr<Sharer>> sharers;
};
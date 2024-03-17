#include "./SharingStrategy.h"
#include "utils/Parameters.h"

ulong SharingStrategy::getSleepingTime()
{
    return Parameters::getIntParam("shr-sleep", 500000);
}

int SharingStrategy::getLiteralsCount(std::vector<ClauseExchange *> clauses)
{
    int count = 0;
    for (auto clause : clauses)
        count += clause->size;
    return count;
}
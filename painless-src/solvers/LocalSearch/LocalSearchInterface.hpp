#pragma once

#include "solvers/SolverInterface.hpp"

enum LocalSearchType
{
    YALSAT = 0,
};

struct LocalSearchStats
{
    unsigned numberUnsatClauses;
    unsigned numberFlips;
    /*TODO add all needed stats */
};

class LocalSearchInterface : public SolverInterface
{
public:
    LocalSearchInterface(int solverId, unsigned typeId, LocalSearchType _lsType) : SolverInterface(SolverAlgorithmType::LOCAL_SEARCH, solverId, typeId), lsType(_lsType)
    {
    }

    ~LocalSearchInterface()
    {
    }

    /// Set initial phase for a given variable.
    virtual void setPhase(const unsigned var, const bool phase) = 0;

    void printWinningLog()
    {
        this->SolverInterface::printWinningLog();
        LOGSTAT("The winner is %u.", this->getSolverId());
    }

    unsigned getNbUnsat() 
    {
        return this->lsStats.numberUnsatClauses;
    }

protected:

    /// @brief Type of the local search
    LocalSearchType lsType;

    /// @brief Some stats on the local search
    LocalSearchStats lsStats;

    /// @brief Vector holding the model or the final trail
    std::vector<int> finalTrail;
};
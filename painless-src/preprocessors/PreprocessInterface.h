#pragma once

#include "utils/ClauseUtils.h"

class PreprocessInterface
{
public:
    /// Constructor.
    PreprocessInterface(int id, unsigned typeId): m_preId(id), m_preTypeId(typeId) {}

    /// Destructor.
    virtual ~PreprocessInterface() {}

    virtual unsigned getVariablesCount() = 0;

    virtual void setInterrupt() = 0;

    virtual void unsetInterrupt() = 0;

    virtual void run() = 0;

    virtual void addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVariables) = 0;

    virtual void loadFormula(const char *filename) = 0;

    virtual void printStatistics() = 0;

    virtual std::vector<simpleClause> getClauses() = 0;

    virtual std::size_t getClausesCount() = 0;

    virtual void printParameters() = 0;

    bool isInitialized() { return this->initialized; }

    virtual void restoreModel(std::vector<int> &model) = 0;

    unsigned getPreTypeId() { return this->m_preTypeId; }
    void setPreTypeId(unsigned typeId) { this->m_preTypeId = typeId; }

    unsigned getPreId() { return this->m_preId; }
    void setPreId(unsigned id) { this->m_preId = id; }

protected:
    /// @brief When the preprocessing can be started
    bool initialized = false;

    /// @brief An id local to the type for better control on the diversification. In Distributed mode, m_preTypeId is not updated as the main m_preId
    unsigned m_preTypeId;

    /// @brief Main id of the preprocessor
    int m_preId;
};
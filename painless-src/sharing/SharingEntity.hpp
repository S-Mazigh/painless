#pragma once

#include "sharing/SharingEntityVisitor.h"
#include "clauses/ClauseExchange.h"
#include "utils/Logger.h"

#include <vector>
#include <memory>
#include <atomic>

/// @brief A base class representing the entities that can exchange clauses via the local sharer(s)
/// \ingroup sharing
class SharingEntity
{
public:
    SharingEntity() : m_sharingId(currentSharingId.fetch_add(1)) {
        LOGDEBUG1("I am sharing entity %d", m_sharingId);
    }

    virtual ~SharingEntity() {}

    /// @brief to get clauses from this sharing entity
    /// @param v_clauses the vector to fill with clauses
    virtual void exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_clauses) = 0;

    /// @brief to add a clause to this sharing entity
    /// @param clause the clause to add
    /// @return true if clause was imported, otherwise false
    virtual bool importClause(std::shared_ptr<ClauseExchange> clause) = 0;

    /// @brief to add multiple clauses to this sharing entity
    /// @param v_clauses the vector with the clauses to add
    virtual void importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &v_clauses) = 0;

    /**
     * @brief to set the lbdLimit on clauses to export
     * @param lbdValue the new lbdLimit value
     */
    virtual void setLbdLimit(unsigned lbdValue)
    {
        this->lbdLimit = lbdValue;
    }

    /// @brief Get the sharing id
    /// @return the sharing id
    int getSharingId() { return this->m_sharingId; }

    /// @brief Set the sharing id
    /// @param _id the new sharing id
    void setSharingId(int _id) { this->m_sharingId = _id; }

    /// @brief The method used to call the SharingEntityVisitor's method
    /// @param v The SharingEntityVisitor that implements methods on this object
    virtual void accept(SharingEntityVisitor *v) { v->visit(this); }

    /// Lbd value the clauses mustn't exceed at export
    std::atomic<unsigned> lbdLimit;

private:
    int m_sharingId;

    static std::atomic<int> currentSharingId;
};
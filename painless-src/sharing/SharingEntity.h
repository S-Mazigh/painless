#pragma once

#include "sharing/SharingEntityVisitor.h"
#include "utils/Logger.h"
#include <vector>
#include "clauses/ClauseExchange.h"

/// @brief A base class representing the entities that can exchange clauses via the local sharer(s)
/// \ingroup sharing
class SharingEntity
{
public:
    SharingEntity(int _id);

    virtual ~SharingEntity();

    /// @brief to get clauses from this sharing entity
    /// @param v_clauses the vector to fill with clauses
    virtual void exportClauses(std::vector<ClauseExchange *> &v_clauses) = 0;

    /// @brief to add a clause to this sharing entity
    /// @param clause the clause to add
    /// @return true if clause was imported, otherwise false
    virtual bool importClause(ClauseExchange *clause) = 0;

    /// @brief to add multiple clauses to this sharing entity
    /// @param v_clauses the vector with the clauses to add
    virtual void importClauses(const std::vector<ClauseExchange *> &v_clauses) = 0;

    /// Increase the counter of references of this entity.
    void increase();

    /// Decrease the counter of references of this entity, delete it if needed.
    void release();

    // virtual bool testStrengthening() = 0;

    /// @brief The method used to call the SharingEntityVisitor's method
    /// @param v The SharingEntityVisitor that implements methods on this object
    virtual void accept(SharingEntityVisitor *v) = 0;

    /// @brief Return the id of this entity
    /// @return the id of this entity as an integer
    inline int getId() const { return id; }

    /// @brief This entity's id
    int id;

    /// Number of references pointing on this sharingEntity.
    atomic<int> nRefs;
};
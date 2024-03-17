#ifndef NDIST

#pragma once

#include "sharing/SharingEntity.h"
#include "clauses/ClauseDatabase.h"

/// @brief The element saving the clauses toSend globally and the received ones
/// \ingroup sharing
class GlobalDatabase : public SharingEntity
{
public:
    // Constructor
    GlobalDatabase(int entity_id, ClauseDatabase *firstDB, ClauseDatabase *secondDB);

    /// Destructor
    ~GlobalDatabase();

    //===================================================================================
    // Local Import/Export interface
    //===================================================================================

    /// Add a clause to clausesToSend == addToSend()
    inline bool importClause(ClauseExchange *cls)
    {
        return this->clausesToSend->addClause(cls); // ref is increased in addClause if the clause is really added
        // bloom filter is left for sharingStrategy when serializing if wanted
    }

    /// Add multiple clauses to clausesToSend
    void importClauses(const std::vector<ClauseExchange *> &v_cls);

    /// Give all the received clauses from receivedClauses, == getReceived()
    void exportClauses(std::vector<ClauseExchange *> &v_cls);

    /// @brief Give a limited number of clauses to not consume all the clauses and possibly loose them in the selection
    /// @param v_cls vector to hold the clauses selected
    /// @param totalSize the number of literals to return
    void exportClauses(std::vector<ClauseExchange *> &v_cls, uint totalSize);

    //===================================================================================
    // Add/Get Interface for GlobalSharingStrategy
    //===================================================================================

    /// @brief Add one clause to the received database
    /// @param cls the clause to add
    inline bool addReceivedClause(ClauseExchange *cls)
    {
        return this->clausesReceived->addClause(cls);
    }

    /// @brief To add a set of clauses to clausesReceived
    /// @param v_cls the vector of clauses to add
    void addReceivedClauses(std::vector<ClauseExchange *> &v_cls);

    /// @brief to get all the clauses in clausesToSend
    /// @param v_cls the vector that will contain the clauses from clausesToSend
    void getClausesToSend(std::vector<ClauseExchange *> &v_cls);

    /// @brief to get a limited number of the clauses in clausesToSend
    /// @param v_cls the vector that will contain the clauses from clausesToSend
    /// @param literals_count the maximum number of literals to return
    void getClausesToSend(std::vector<ClauseExchange *> &v_cls, uint literals_count);

    /// @brief to get the best clause in the clauseToSend database
    /// @param cls a double pointer, since I seek to get a pointer value
    /// @return true if found at least one clause to select, otherwise false
    inline bool getClauseToSend(ClauseExchange **cls)
    {
        return clausesToSend->giveOneClause(cls);
    }

    //==============================================================
    // Diverse methods
    //==============================================================

    /// @brief The method that will call the right visit method of the SharingEntityVisitor v
    /// @param v The SharingEntityVisitor that defines what should be done on this global sharer
    inline void accept(SharingEntityVisitor *v) override { v->visit(this); }

    // inline bool testStrengthening() override { return false; }

    inline void clear()
    {
        clausesToSend->deleteClauses();
        clausesReceived->deleteClauses();
    }

protected:
    /// A collection of clauses that may be exported, the clauses are dereferenced before sharing.
    ClauseDatabase *clausesToSend;

    /// A collection of received clauses, to be accessed by the sharers
    ClauseDatabase *clausesReceived;
};

#endif
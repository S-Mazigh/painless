// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------
#include "SimpleSharing.h"

#include "clauses/ClauseManager.h"
#include "solvers/SolverFactory.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "painless.h"

SimpleSharing::SimpleSharing(int id, std::vector<SharingEntity *> &producers, std::vector<SharingEntity *> &consumers) : LocalSharingStrategy(id, producers, consumers)
{
    // shr-lit * solver_nbr
    this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
    this->initPhase = true;
    // Init database
    this->database = new ClauseDatabaseVector();

    if (Parameters::getBoolParam("dup"))
    {
        this->filter = new BloomFilter();
    }
}

SimpleSharing::SimpleSharing(int id, std::vector<SharingEntity *> &&producers, std::vector<SharingEntity *> &&consumers) : LocalSharingStrategy(id, producers, consumers)
{
    // shr-lit * solver_nbr
    this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
    this->initPhase = true;
    // Init database
    this->database = new ClauseDatabaseVector();

    if (Parameters::getBoolParam("dup"))
    {
        this->filter = new BloomFilter();
    }
}

SimpleSharing::~SimpleSharing()
{
    delete database;
    if (Parameters::getBoolParam("dup"))
    {
        delete filter;
    }
}

bool SimpleSharing::doSharing()
{
    if (globalEnding)
        return true;
    // 1- Fill the database using all the producers
    for (auto producer : producers)
    {
        unfiltered.clear();
        filtered.clear();

        if (Parameters::getBoolParam("dup"))
        {
            producer->exportClauses(unfiltered);
            // ref = 1
            for (ClauseExchange *c : unfiltered)
            {
                if (!filter->contains_or_insert(c->lits))
                {
                    filtered.push_back(c);
                }
            }
            stats.receivedClauses += unfiltered.size();
            stats.receivedDuplicas += (unfiltered.size() - filtered.size());
        }
        else
        {
            producer->exportClauses(filtered);
            stats.receivedClauses += filtered.size();
        }

        // if solver checks the usage percentage else nothing fancy to do
        // producer->accept(this);

        for (auto cls : filtered)
        {
            this->database->addClause(cls); // if not added it is released : ref = 0
        }
    }

    unfiltered.clear();
    filtered.clear();
    // 2- Get selection
    // consumer receives the same amount as in the original
    this->database->giveSelection(filtered, literalPerRound * producers.size());

    stats.sharedClauses += filtered.size();

    // 3-Send the best clauses (all producers included) to all consumers
    for (auto consumer : consumers)
    {
        for (auto cls : filtered)
        {
            if (cls->from != consumer->id)
            {
                consumer->importClause(cls);
            }
        }
    }

    // release the clauses of this context
    for (auto cls : filtered)
    {
        ClauseManager::releaseClause(cls);
    }

    LOG(1, "[SimpleSat %d] received cls %ld, shared cls %ld\n", idSharer, stats.receivedClauses, stats.sharedClauses);

    if (globalEnding)
        return true;
    return false;
}

void SimpleSharing::visit(SolverInterface *solver)
{
    LOG(3, "[SimpleSat %d] Visiting the solver %d\n", idSharer, solver->id);
}

void SimpleSharing::visit(SharingEntity *sh_entity)
{
    LOG(3, "[SimpleSat %d] Visiting the sh_entity %d\n", idSharer, sh_entity->id);
}

#ifndef NDIST
void SimpleSharing::visit(GlobalDatabase *g_base)
{
    LOG(3, "[SimpleSat %d] Visiting the global database %d\n", idSharer, g_base->id);

    LOG(1, "[SimpleSat %d] Added %d clauses imported from another process\n", idSharer, filtered.size());
}
#endif
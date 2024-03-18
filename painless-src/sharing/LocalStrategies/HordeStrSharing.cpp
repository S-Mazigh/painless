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
#include "HordeStrSharing.h"
#include "clauses/ClauseManager.h"
#include "solvers/SolverFactory.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/BloomFilter.h"
#include "painless.h"

HordeStrSharing::HordeStrSharing(int id, std::vector<SharingEntity *> &producers, std::vector<SharingEntity *> &consumers, SolverInterface *reducer_) : LocalSharingStrategy(id, producers, consumers), reducer(reducer_)
{
    this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
    this->initPhase = true;
    // number of round corresponding to 5% of the 5000s timeout
    int sleepTime = Parameters::getIntParam("shr-sleep", 500000);
    if (sleepTime)
        this->roundBeforeIncrease = 250000000 / sleepTime;
    else
    {
        LOG(1, "Warning: sleepTime of sharing strategy is set to: %d", sleepTime);
        this->roundBeforeIncrease = 250000000 / 500000;
    }
}

HordeStrSharing::HordeStrSharing(int id, std::vector<SharingEntity *> &&producers, std::vector<SharingEntity *> &&consumers, SolverInterface *reducer_) : LocalSharingStrategy(id, producers, consumers), reducer(reducer_)
{
    this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
    this->initPhase = true;
    // number of round corresponding to 5% of the 5000s timeout
    int sleepTime = Parameters::getIntParam("shr-sleep", 500000);
    if (sleepTime)
        this->roundBeforeIncrease = 250000000 / sleepTime;
    else
    {
        LOG(1, "Warning: sleepTime of sharing strategy is set to: %d", sleepTime);
        this->roundBeforeIncrease = 250000000 / 500000;
    }
}

HordeStrSharing::~HordeStrSharing()
{
    for (auto pair : this->databases)
    {
        delete pair.second;
    }
}

bool HordeStrSharing::doSharing()
{
    if (globalEnding)
        return true;

    // Producers to reducer
    for (auto producer : producers)
    {
        if (!this->databases.count(producer->id))
        {
            this->databases[producer->id] = new ClauseDatabaseVector();
        }
        tmp.clear();

        producer->exportClauses(tmp);
        stats.receivedClauses += tmp.size();

        for (auto cls : tmp)
        {
            this->databases[producer->id]->addClause(cls); // if not added it is released : ref = 0
        }
        tmp.clear();

        // get selection and checks the used percentage if producer is a solver
        producer->accept(this);

        stats.sharedClauses += tmp.size();

        reducer->importClauses(tmp); // if imported ref++

        // release the selection
        for (ClauseExchange *cls : tmp)
        {
            ClauseManager::releaseClause(cls);
        }
    }

    // Reducer to consumers
    if (!this->databases.count(reducer->id))
    {
        this->databases[reducer->id] = new ClauseDatabaseVector();
    }
    tmp.clear();

    reducer->exportClauses(tmp);
    stats.receivedClauses += tmp.size();

    for (auto cls : tmp)
    {
        this->databases[reducer->id]->addClause(cls); // if not added it is released : ref = 0
    }
    tmp.clear();

    // get selection and checks the used percentage if producer is a solver
    reducer->accept(this);

    stats.sharedClauses += tmp.size();

    for (auto consumer : consumers)
    {
        consumer->importClauses(tmp); // if imported ref++
    }
    round++;

    LOG(1, "[HordeStr %d] received cls %ld, shared cls %ld\n", idSharer, stats.receivedClauses, stats.sharedClauses);
    if (globalEnding)
        return true;

    return false;
}

void HordeStrSharing::visit(SolverInterface *solver)
{
    LOG(3, "[HordeSat %d] Visiting the solver %d\n", idSharer, solver->id);
    int used, usedPercent, selectCount;

    used = this->databases[solver->id]->giveSelection(tmp, literalPerRound, &selectCount);
    usedPercent = (100 * used) / literalPerRound;

    stats.sharedClauses += tmp.size();

    if (usedPercent < 75)
    {
        solver->increaseClauseProduction();
        LOG(3, "[HordeStr %d] production increase for solver %d.\n", idSharer, solver->id);
    }
    else if (usedPercent > 98)
    {
        solver->decreaseClauseProduction();
        LOG(3, "[HordeStr %d] production decrease for solver %d.\n", idSharer, solver->id);
    }

    if (selectCount > 0)
    {
        LOG(3, "[HordeStr %d] filled %d%% of its buffer %.2f\n", idSharer, usedPercent, used / (float)selectCount);
        this->initPhase = false;
    }
}

void HordeStrSharing::visit(SharingEntity *sh_entity)
{
    LOG(3, "[HordeSat %d] Visiting the sharing entity %d\n", idSharer, sh_entity->id);

    this->databases[sh_entity->id]->giveSelection(tmp, literalPerRound);
}

#ifndef NDIST

void HordeStrSharing::visit(GlobalDatabase *g_base)
{
    LOG(3, "[HordeSat %d] Visiting the global database %d\n", idSharer, g_base->id);

    this->databases[g_base->id]->giveSelection(tmp, literalPerRound);

    LOG(1, "[HordeSat %d] Added %d clauses imported from another process\n", idSharer, tmp.size());
}
#endif
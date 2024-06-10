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

#pragma once

#include "clauses/ClauseExchange.h"
#include "utils/Logger.h"
#include "utils/ClauseUtils.h"

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <memory>
#include <random>
#include <atomic>

#define ID_SYM 0
#define ID_XOR 1

/// Code for SAT result
enum class SatResult
{
   SAT = 10,
   UNSAT = 20,
   TIMEOUT = 30,
   UNKNOWN = 0
};

/// @brief Code for the type of algorithm of solvers
enum class SolverAlgorithmType
{
   CDCL = 0,
   LOCAL_SEARCH = 1,
   LOOK_AHEAD = 2,
   OTHER = 3,
   UNKNOWN = 255,
};

/// \defgroup solving Solving/Preprocessing Related Classes
/// \ingroup solving

/// @brief Interface of a solver that provides standard features.
class SolverInterface
{
public:
   /// Get the current number of variables.
   virtual int getVariablesCount() = 0;

   /// Get a variable suitable for search splitting.
   virtual int getDivisionVariable() = 0;

   /// Interrupt resolution, solving cannot continue until interrupt is unset.
   virtual void setSolverInterrupt() = 0;

   /// Remove the SAT solving interrupt request.
   virtual void unsetSolverInterrupt() = 0;

   /// So that the working strategies support all SolverInterface's.
   virtual void printWinningLog()
   {
      int algo = static_cast<int>(this->m_algoType);
      LOGSTAT("The winner is of type: %s", (algo) ? (algo) ? "LOCAL_SEARCH" : "PREPROCESSING" : "CDCL");
   }

   /// Solve the formula with a given cube.
   virtual SatResult solve(const std::vector<int> &cube) = 0;

   /// Add a permanent clause to the formula.
   virtual void addClause(std::shared_ptr<ClauseExchange> clause) = 0;

   /// Add a list of permanent clauses to the formula.
   virtual void addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses) = 0;

   /// Add a list of initial clauses to the formula.
   virtual void addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars) = 0;

   /// Load formula from a given dimacs file, return false if failed.
   virtual void loadFormula(const char *filename) = 0;

   /// Print solver statistics.
   virtual void printStatistics()
   {
      LOGWARN("printStatistics not implemented");
   }

   /// Return the model in case of SAT result.
   virtual std::vector<int> getModel() = 0;

   /// @brief Prints the parameters set for a solver;
   virtual void printParameters()
   {
      LOGWARN("printParameters not implemented");
   }

   /// Native diversification.
   /// @param noise added for some radomness
   virtual void diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist) = 0;

   bool isInitialized() { return this->m_initialized; }
   void setInitialized(bool value) { this->m_initialized = value; }

   SolverAlgorithmType getAlgoType() { return this->m_algoType; }

   unsigned getSolverTypeId() { return this->m_solverTypeId; }
   void setSolverTypeId(unsigned typeId) { this->m_solverTypeId = typeId; }

   unsigned getSolverId() { return this->m_solverId; }
   void setSolverId(unsigned id) { this->m_solverId = id; }

   /// Constructor.
   SolverInterface(SolverAlgorithmType algoType, int solverId, unsigned typeId) : m_algoType(algoType), m_solverTypeId(typeId), m_solverId(solverId), m_initialized(false)
   {}

   /// Destructor.
   virtual ~SolverInterface()
   {
   }

protected:
   /// Type of this solver.
   SolverAlgorithmType m_algoType;

   /// @brief This bool can be used to detect wrong usage of the interface.
   std::atomic<bool> m_initialized;

   /// @brief An id local to the type for better control on the diversification. In Distributed mode, m_solverTypeId is not updated as the main m_solverId
   unsigned m_solverTypeId;

   /// @brief The main id of the solver
   int m_solverId;
};

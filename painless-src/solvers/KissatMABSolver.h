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

#include <iostream>
#include <fstream>
#include <unordered_map>
#include "clauses/ClauseBuffer.h"
#include "../solvers/SolverInterface.hpp"
#include "utils/Threading.h"

// Some forward declatarations for KissatMABSolver
struct kissat;
struct cvec;

/// Instance of a KissatMABSolver solver
class KissatMABSolver : public SolverInterface
{
public:
   void addOriginClauses(simplify *S);

   void setBumpVar(int v);

   ofstream outputf;
   /// Load formula from a given dimacs file, return false if failed.
   bool loadFormula(const char *filename);

   /// Get the number of variables of the current resolution.
   int getVariablesCount();

   /// Get a variable suitable for search splitting.
   int getDivisionVariable();

   /// Set initial phase for a given variable.
   void setPhase(const int var, const bool phase);

   /// Bump activity of a given variable.
   void bumpVariableActivity(const int var, const int times);

   /// Interrupt resolution, solving cannot continue until interrupt is unset.
   void setSolverInterrupt();

   /// Remove the SAT solving interrupt request.
   void unsetSolverInterrupt();

   /// Solve the formula with a given cube.
   SatResult solve(const std::vector<int> &cube);

   /// Add a permanent clause to the formula.
   void addClause(ClauseExchange *clause);

   /// Add a list of permanent clauses to the formula.
   void addClauses(const std::vector<ClauseExchange *> &clauses);

   /// Add a list of initial clauses to the formula.
   void addInitialClauses(const std::vector<ClauseExchange *> &clauses);

   /// Add a learned clause to the formula.
   bool importClause(ClauseExchange *clause);

   /// Add a list of learned clauses to the formula.
   void importClauses(const std::vector<ClauseExchange *> &clauses);

   /// Get a list of learned clauses.
   void exportClauses(std::vector<ClauseExchange *> &clauses);

   /// Request the solver to produce more clauses.
   void increaseClauseProduction();

   /// Request the solver to produce less clauses.
   void decreaseClauseProduction();

   void setParameter(parameter p);

   /// Get solver statistics.
   SolvingStatistics getStatistics();

   /// Return the model in case of SAT result.
   std::vector<int> getModel();

   /// Native diversification.
   void diversify();

   /// @brief Initializes the map @ref kissatOptions with the default configuration.
   void initkissatMABOptions();

   /// Native diversification.
   void initshuffle(int id);

   /// Constructor.
   KissatMABSolver(int id);

   /// Destructor.
   virtual ~KissatMABSolver();

   std::vector<int> getFinalAnalysis();

   std::vector<int> getSatAssumptions();

protected:
   int bump_var;

   /// Pointer to a KissatMABSolver solver.
   kissat *solver;

   /// @brief A map mapping a kissat option name to its value.
   std::unordered_map<std::string, int> kissatMABOptions;

   /// Buffer used to import clauses (units included).
   ClauseBuffer clausesToImport;
   ClauseBuffer unitsToImport;

   /// Buffer used to export clauses (units included).
   ClauseBuffer clausesToExport;

   /// Buffer used to add permanent clauses.
   ClauseBuffer clausesToAdd;

   /// Size limit used to share clauses.
   atomic<int> lbdLimit;

   /// Used to stop or continue the resolution.
   atomic<bool> stopSolver;

   /// Callback to export/import clauses.
   friend int cbkKissatMABSolverImportUnit(void *);
   friend int cbkKissatMABSolverImportClause(void *, unsigned int *, cvec *);
   friend void cbkKissatMABSolverExportClause(void *, unsigned int, cvec *);
};

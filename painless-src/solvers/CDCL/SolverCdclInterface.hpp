#pragma once

#include "solvers/SolverInterface.hpp"
#include "sharing/SharingEntity.hpp"

/// Code  for the type of solvers
enum class SolverCdclType
{
   GLUCOSE = 0,
   LINGELING = 1,
   MAPLE = 2,
   MINISAT = 3,
   KISSAT = 4,
   MAPLECOMSPS = 5,
   GKISSAT = 6,
   REDUCER = 10,
};

/// Structure for solver statistics
struct SolvingCdclStatistics
{
   /// Constructor
   SolvingCdclStatistics()
   {
      propagations = 0;
      decisions = 0;
      conflicts = 0;
      restarts = 0;
      memPeak = 0;
   }

   unsigned long propagations; ///< Number of propagations.
   unsigned long decisions;    ///< Number of decisions taken.
   unsigned long conflicts;    ///< Number of reached conflicts.
   unsigned long restarts;     ///< Number of restarts.
   double memPeak;             ///< Maximum memory used in Ko.
};

/// \ingroup solving

class SolverCdclInterface : public SolverInterface, public SharingEntity
{
public:
   /// Set initial phase for a given variable.
   virtual void setPhase(const unsigned var, const bool phase) = 0;

   /// Bump activity of a given variable.
   virtual void bumpVariableActivity(const int var, const int times) = 0;

   /// Request the solver to produce more clauses.
   virtual void increaseClauseProduction() = 0;

   /// Request the solver to produce less clauses.
   virtual void decreaseClauseProduction() = 0;

   /// Return the final analysis in case of UNSAT result.
   virtual std::vector<int> getFinalAnalysis() = 0;

   /// @brief ??
   /// @return  ??
   virtual std::vector<int> getSatAssumptions() = 0;

   void printWinningLog()
   {
      this->SolverInterface::printWinningLog();
   }

   /// Constructor.
   SolverCdclInterface(int solverId, unsigned typeId, SolverCdclType solverCdclType) : SolverInterface(SolverAlgorithmType::CDCL, solverId, typeId), SharingEntity(), m_cdclType(solverCdclType)
   {
      LOGDEBUG1("I am solver %d, typeId: %u", m_solverId, m_solverTypeId);
   }

   /// @brief The method that will call the right visit method of the SharingEntityVisitor v.
   /// @param v The SharingEntityVisitor that defines what should be done on this solver
   void accept(SharingEntityVisitor *v) override
   {
      v->visit(this);
   }

   /// Destructor.
   virtual ~SolverCdclInterface()
   {
   }

   /// Type of this solver.
   SolverCdclType m_cdclType;
};

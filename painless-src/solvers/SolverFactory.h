#pragma once

#include "solvers/CDCL/SolverCdclInterface.hpp"
#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include <vector>
#include <atomic>



/// \ingroup solving
/// @brief Factory to create solvers.
class SolverFactory
{
public:
   static SolverAlgorithmType createSolver(char type, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers);
   
   static SolverAlgorithmType createSolver(char type, std::shared_ptr<SolverInterface> &createdSolver);

   static void createSolvers(int count, std::string portfolio, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers);

   /// @brief 
   /// @param groupSize 
   /// @param portfolio 
   /// @param cdclSolvers 
   /// @param localSolvers 
   /// @return false when not all count solvers were able to be instantiated.
   static void createInitSolvers(int count, std::string portfolio, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers, std::vector<simpleClause> &initClauses, unsigned varCount);

   /// Print stats of a groupe of solvers.
   static void printStats(const std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, const std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers);

   /// Apply a diversification on solvers.
   static void diversification(const std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, const std::vector<std::shared_ptr<LocalSearchInterface>> &localSolvers);

public:
   static std::atomic<int> currentIdSolver;

   static unsigned typesCount;
};

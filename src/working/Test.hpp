#pragma once

#include "WorkingStrategy.hpp"

class Test : public WorkingStrategy
{
  public:
	Test();

	~Test();

	void solve(const std::vector<int>& cube);

	void join(WorkingStrategy* strat, SatResult res, const std::vector<int>& model);

	void restoreModelDist();

	void setSolverInterrupt();

	void unsetSolverInterrupt();

	void waitInterrupt();

  protected:
	std::atomic<bool> strategyEnding;
};

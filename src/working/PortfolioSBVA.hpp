#pragma once

#include "utils/Parameters.hpp"
#include "working/WorkingStrategy.hpp"

#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include "preprocessors/PRS-Preprocessors/preprocess.hpp"
#include "preprocessors/StructuredBva.hpp"

#include "sharing/GlobalStrategies/GlobalSharingStrategy.hpp"
#include "sharing/SharingStrategy.hpp"

#include "sharing/Sharer.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

class PortfolioSBVA : public WorkingStrategy
{
  public:
	PortfolioSBVA();

	~PortfolioSBVA();

	void solve(const std::vector<int>& cube);

	void join(WorkingStrategy* strat, SatResult res, const std::vector<int>& model);

	void setSolverInterrupt();

	void stopSbva();

	void joinSbva();

	void unsetSolverInterrupt();

	void waitInterrupt();

	bool isStrategyEnding() { return strategyEnding; }

  protected:
	// Strategy Management
	//--------------------

	/* All this is because I didn't want to change SequentialWorker or make SBVA inherits SolverInterface: big changes
	 * are coming */
	std::atomic<bool> strategyEnding;
	std::mutex sbvaLock;
	std::condition_variable sbvaSignal;
	/* Not atomic to force use of sbvaLock */
	unsigned int sbvaEnded = 0;

	// Workers
	//--------

	std::vector<std::thread> sbvaWorkers;
	std::vector<std::thread> clausesLoad;
	std::vector<std::shared_ptr<StructuredBVA>> sbvas;
	preprocess prs{ -1 };

	/* Mutex needed to manage the workers: the mutex synchronize the workers, the main thread and the portfolio thread
	 */
	Mutex workersLock;

	// Data
	//-----

	/* Memory is not freed at clear !! : To move to solve */
	std::vector<simpleClause> initClauses;
	unsigned int beforeSbvaVarCount = 0;
	int leastClausesIdx = -1;

	// Diversification
	//----------------

	std::mt19937 engine{ std::random_device{}() };
	std::uniform_int_distribution<int> uniform{ 1, __globalParameters__.maxDivNoise };

	// Sharing
	//--------

	/* Use weak_ptr instead of shared_ptr ? */
	std::vector<std::shared_ptr<SharingStrategy>> localStrategies;
	std::vector<std::shared_ptr<GlobalSharingStrategy>> globalStrategies;
	std::vector<std::unique_ptr<Sharer>> sharers;
};

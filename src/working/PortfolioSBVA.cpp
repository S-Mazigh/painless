#include "painless.hpp"

#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/NumericConstants.hpp"
#include "utils/Parameters.hpp"
#include "utils/System.hpp"

#include "working/PortfolioSBVA.hpp"
#include "working/SequentialWorker.hpp"

#include "solvers/SolverFactory.hpp"

#include "sharing/SharingStrategyFactory.hpp"

#include <limits.h>

void
runSBVA(std::mt19937& engine,
		std::uniform_int_distribution<int>& uniform,
		PortfolioSBVA* master,
		std::vector<simpleClause>& initClauses,
		unsigned int initVarCount,
		std::shared_ptr<StructuredBVA> sbva)
{
	// Parse
	std::vector<simpleClause> clauses;

	sbva->addInitialClauses(initClauses, initVarCount);

	if (!sbva->isInitialized()) {
		LOGWARN("SBVA %d was not initialized in time thus returning !", sbva->getSolverId());
		master->joinSbva();
		return;
	}

	sbva->solve();
	// sbva->printStatistics();

	LOG2("Sbva %d ended", sbva->getSolverId());
	sbva->printStatistics();

	master->joinSbva();
}

PortfolioSBVA::PortfolioSBVA() {}

PortfolioSBVA::~PortfolioSBVA()
{
	/* Must be after the setSolverInterrupt of sbva and lsSolver, and cannot be in Interrupt since it can be called from
	 * workers */
	for (auto& worker : clausesLoad)
		worker.join();

	for (auto& worker : sbvaWorkers)
		worker.join();

	for (size_t i = 0; i < slaves.size(); i++) {
		delete slaves[i];
	}
}

void
PortfolioSBVA::solve(const std::vector<int>& cube)
{
	LOG0(">> PortfolioSBVA");

	double approxMemoryPerSolver;

	// Timeout management
	unsigned int sbvaTimeout = __globalParameters__.sbvaTimeout;

	unsigned int nbSBVA = 12;
	int nbLs = __globalParameters__.sbvaPostLocalSearchers;
	int initNbSlaves = __globalParameters__.cpus - nbSBVA;

	/* TODO: I should really generalize sequentialWorker and extend Factory to simplify all this mess */

	std::vector<std::shared_ptr<SolverCdclInterface>> cdclSolvers;
	std::vector<std::shared_ptr<LocalSearchInterface>> localSolvers;

	if (initNbSlaves < 0) {
		LOGERROR("The specified number of threads %d is less than the wanted number of sbvas %d !!",
				 __globalParameters__.cpus,
				 nbSBVA);
		exit(PERR_ARGS_ERROR);
	}

	this->strategyEnding = false;

	// Parsers::parseCNF(__globalParameters__.filename.c_str(), this->initClauses, &this->beforeSbvaVarCount);

	/* PRS */
	prs.loadFormula(__globalParameters__.filename.c_str());
	SatResult res = prs.solve();
	LOGDEBUG1("PRS returned %d", res);
	if (SatResult::UNSAT == res) {
		LOG0("PRS answered UNSAT");
		finalResult = SatResult::UNSAT;
		this->join(this, finalResult, {});
		return;
	} else if (SatResult::SAT == res) {
		LOG0("PRS answered SAT");
		for (unsigned int i = 1; i <= prs.vars; i++) {
			finalModel.push_back(prs.model[i]);
		}
		finalResult = SatResult::SAT;
		this->join(this, finalResult, finalModel);
		return;
	}
	// Free some memory
	this->prs.releaseMemory();

	// PRS uses indexes from 1 to prs.clauses
	this->initClauses = std::move(prs.clause);
	this->initClauses.erase(this->initClauses.begin());
	this->beforeSbvaVarCount = prs.vars;

	if (nbSBVA && initClauses.size() > __globalParameters__.sbvaMaxClause) {
		nbSBVA = 1;
		sbvaTimeout = 2000;
		initNbSlaves = __globalParameters__.cpus - nbSBVA;
		nbLs = 0;
		LOGWARN(
			"Too much clauses, sbva will be used only for one kissat with timeout %d. No Yalsat will be instantiated",
			sbvaTimeout);
	}

	/* ~solver mem + learned clauses ~= reduction_ratio*PRS memory : this is a vague approximation!*/
	double reductionRatio = (((double)prs.vars / prs.orivars) * ((double)prs.clauses / prs.oriclauses));
	approxMemoryPerSolver = 1.2 * reductionRatio * SystemResourceMonitor::getUsedMemoryKB();

	LOG1("Reduction Ratio: %f, Memory : %f", reductionRatio, approxMemoryPerSolver);

	// Add initial slaves
	for (unsigned int i = 1; i <= initNbSlaves; i++) {
		/* i * solverMem */
		LOGDEBUG1("Memory: %lu / %lu. %lu / %lu",
				  SystemResourceMonitor::getUsedMemoryKB() + i * approxMemoryPerSolver,
				  SystemResourceMonitor::getTotalMemoryKB(),
				  i * approxMemoryPerSolver,
				  SystemResourceMonitor::getAvailableMemoryKB());
		if (SystemResourceMonitor::getAvailableMemoryKB() <= i * approxMemoryPerSolver) {
			LOGERROR("SolverFactory cannot instantiate the %d th solver (%f/%f) due to insufficient memory (to be "
					 "used: %lu / %lu)",
					 i,
					 i * approxMemoryPerSolver,
					 SystemResourceMonitor::getAvailableMemoryKB(),
					 SystemResourceMonitor::getUsedMemoryKB() + i * approxMemoryPerSolver,
					 SystemResourceMonitor::getTotalMemoryKB());
			initNbSlaves = i;
			nbSBVA = 0;
			nbLs = 0;
			break;
		}
		SolverFactory::createSolver('k', __globalParameters__.importDB[0], cdclSolvers, localSolvers);
	}

	// Launch sbva
	if (nbSBVA > 1) {
		for (unsigned int i = 0; i < nbSBVA; i++) {
			this->sbvas.emplace_back(new StructuredBVA(
				initNbSlaves + i, __globalParameters__.sbvaMaxAdd, !__globalParameters__.sbvaNoShuffle));
			this->sbvas.back()->diversify();
		}
	} else if (1 == nbSBVA && (SystemResourceMonitor::getAvailableMemoryKB() > approxMemoryPerSolver)) {
		this->sbvas.emplace_back(
			new StructuredBVA(initNbSlaves, __globalParameters__.sbvaMaxAdd, !__globalParameters__.sbvaNoShuffle));
		this->sbvas.back()->setTieBreakHeuristic(SBVATieBreak::MOSTOCCUR); /* THREEHOPS uses too much memory */
																		   /* Do not call diversify */
	} else {
		nbSBVA = 0;
		LOG0("No SBVA will be executed");
	}

	/* Diversification must be done before addInitialClauses because of kissat_reserve (options changes the required
	 * memory) */
	/* Do not diversify localSearch not even srand */
	SolverFactory::diversification(cdclSolvers, {});

	/* Load clauses */
	for (auto cdcl : cdclSolvers) {
		this->addSlave(new SequentialWorker(cdcl));
		clausesLoad.emplace_back(
			[&cdcl, this] { cdcl->addInitialClauses(this->initClauses, this->beforeSbvaVarCount); });
	}

	/* Sharing */
	/* ------- */
	SharingStrategyFactory::instantiateLocalStrategies(
		__globalParameters__.sharingStrategy, this->localStrategies, cdclSolvers);

	if (dist) {
		SharingStrategyFactory::instantiateGlobalStrategies(__globalParameters__.globalSharingStrategy,
															globalStrategies);
	}

	std::vector<std::shared_ptr<SharingStrategy>> sharingStrategiesConcat;

	/* Launch SBVA */
	/* ----------- */

	for (unsigned int i = 0; i < nbSBVA; i++) {
		sbvaWorkers.emplace_back(runSBVA,
								 std::ref(this->engine),
								 std::ref(this->uniform),
								 this,
								 std::ref(initClauses),
								 this->beforeSbvaVarCount,
								 this->sbvas[i]);
	}

	for (auto& worker : clausesLoad)
		worker.join();

	LOG0("Loaded clauses in initial %d kissats", initNbSlaves);

	/* TODO: asynchronous exit for better management than all the ifs used before each major step */
	if (globalEnding) {
		this->setSolverInterrupt();
		return;
	}

	clausesLoad.clear();
	cdclSolvers.clear();
	localSolvers.clear();

	/* Launch Initial Solvers */
	for (auto slave : slaves)
		slave->solve(cube);

	/* Launch sharers */
	for (auto lstrat : localStrategies) {
		sharingStrategiesConcat.push_back(lstrat);
	}
	for (auto gstrat : globalStrategies) {
		sharingStrategiesConcat.push_back(gstrat);
	}

	SharingStrategyFactory::launchSharers(sharingStrategiesConcat, this->sharers);

	sharingStrategiesConcat.clear();

	if (!sbvas.size()) {
		LOGWARN("No sbvas added, launching only kissats");
		return;
	}

	/* All this exists because I didn't want to change SequentialWorker or make SBVA inherits SolverInterface: big
	 * changes are coming */
	if (!globalEnding) {
		std::unique_lock<std::mutex> lock(this->sbvaLock);
		auto ret = this->sbvaSignal.wait_for(lock, std::chrono::seconds(sbvaTimeout));
		LOGWARN("Portfolio SBVA wakeupRet = %d , globalEnding = %d ", ret, globalEnding.load());
	}

	/* no else to execute after wakeup */
	if (globalEnding) {
		this->setSolverInterrupt();
		return;
	}

	this->stopSbva();

	// choose best; clear clauses of the losers
	for (auto& worker : this->sbvaWorkers)
		worker.join();

	sbvaWorkers.clear();

	/* Didn't use [0] since sbva[0] may be unantialized, must use <= & >= */
	unsigned int leastClauses = UINT_MAX;
	unsigned int sbvaVarCount = 0;

	this->leastClausesIdx = -1;
	for (unsigned int i = 0; i < nbSBVA; i++) {
		if (this->sbvas[i]->isInitialized() &&
			this->sbvas[i]->getPreprocessorStatistics().newFormulaSize < leastClauses) {
			LOGDEBUG1("SBVA %d is initialized (%d)", this->sbvas[i]->getSolverId(), this->sbvas[i]->isInitialized());
			leastClauses = this->sbvas[i]->getPreprocessorStatistics().newFormulaSize;
			leastClausesIdx = i;
		}
	}

	if (globalEnding) {
		this->setSolverInterrupt();
		return;
	}

	/* TODO: redo the local search nbunsat with different techniques ? */
	std::vector<simpleClause> bestClauses;

	if (leastClausesIdx >= 0) {
		bestClauses = std::move(this->sbvas[leastClausesIdx]->getSimplifiedFormula());
		sbvaVarCount = this->sbvas[leastClausesIdx]->getVariablesCount();
		LOG0("Best SBVA according to nbClausesDeleted is : %d", this->sbvas[leastClausesIdx]->getSolverId());
	} else {
		LOGWARN("All SBVAS were not initialized, thus launching with initial clauses");
		bestClauses = std::move(initClauses);
		sbvaVarCount = this->beforeSbvaVarCount;
	}

	initClauses.clear();

	if (nbLs > 0) {
		if (nbLs > nbSBVA) {
			LOGWARN(
				"The number of local searches %d is greater than the number of sbvas %d. Only locals will be launched",
				nbLs,
				nbSBVA);
			nbLs = nbSBVA; // only locals
		}
		for (unsigned int i = 0; i < nbLs; i++) {
			SolverFactory::createSolver('y', __globalParameters__.importDB[0], cdclSolvers, localSolvers);
		}
		nbSBVA -= nbLs;
	}

	if (globalEnding) {
		this->setSolverInterrupt();
		return;
	}

	/* Instantiate new solvers */
	for (unsigned int i = 0; i < nbSBVA; i++) {
		/* Didn't use factory because of Id */
		SolverFactory::createSolver('k', __globalParameters__.importDB[0], cdclSolvers, localSolvers);
	}

	// Diversification must be done before adding clauses
	IDScaler globalIDScaler;
	IDScaler typeIDScaler;
	/* TODO separate Local And Cdcl diversification */
	if (dist) {
		globalIDScaler = [rank = mpi_rank,
						  size = __globalParameters__.cpus](const std::shared_ptr<SolverInterface>& solver) {
			return rank * size + solver->getSolverId();
		};
		typeIDScaler = [rank = mpi_rank,
						size = __globalParameters__.cpus](const std::shared_ptr<SolverInterface>& solver) {
			return rank * solver->getSolverTypeCount() + solver->getSolverTypeId();
		};
	} else {
		globalIDScaler = [](const std::shared_ptr<SolverInterface>& solver) { return solver->getSolverId(); };
		typeIDScaler = [](const std::shared_ptr<SolverInterface>& solver) { return solver->getSolverTypeId(); };
	}
	SolverFactory::diversification(cdclSolvers, localSolvers, globalIDScaler, typeIDScaler);

	for (auto local : localSolvers) {
		clausesLoad.emplace_back(
			[&local, &bestClauses, sbvaVarCount] { local->addInitialClauses(bestClauses, sbvaVarCount); });
	}
	for (auto cdcl : cdclSolvers) {
		clausesLoad.emplace_back(
			[&cdcl, &bestClauses, sbvaVarCount] { cdcl->addInitialClauses(bestClauses, sbvaVarCount); });
	}

	// Wait for clauses init
	for (auto& worker : clausesLoad)
		worker.join();

	clausesLoad.clear();

	if (globalEnding) {
		this->setSolverInterrupt();
		return;
	}

	bestClauses.clear();

	/* Update slaves and clear uneeded workers */
	this->workersLock.lock();
	if (globalEnding) {
		this->workersLock.unlock();
		return;
	}

	for (auto newSolver : cdclSolvers) {
		this->addSlave(new SequentialWorker(newSolver));
	}
	for (auto newLocal : localSolvers) {
		this->addSlave(new SequentialWorker(newLocal));
	}

	SharingStrategyFactory::addEntitiesToLocal(this->localStrategies, cdclSolvers);

	cdclSolvers.clear();
	localSolvers.clear();

	// clear all sbvas to free some memory
	this->sbvas.clear();

	this->workersLock.unlock();

	// Launch slaves
	for (unsigned int i = initNbSlaves; i < slaves.size(); i++) {
		slaves[i]->solve({});
	}

	// Not needed anymore
	localStrategies.clear();
	globalStrategies.clear();
}

void
PortfolioSBVA::join(WorkingStrategy* strat, SatResult res, const std::vector<int>& model)
{
	LOGDEBUG1("SBVA joining from slave %p with res %d. GlobalEnding = %d, StrategyEnding = %d",
			  strat,
			  res,
			  globalEnding.load(),
			  strategyEnding.load());

	if (res == SatResult::UNKNOWN || strategyEnding || globalEnding)
		return;

	strategyEnding = true;

	setSolverInterrupt();

	LOGDEBUG1("Interrupted all workers");

	if (parent == NULL) { // If it is the top strategy

		globalEnding = true;

		finalResult = res;

		if (res == SatResult::SAT) {
			finalModel = model; /* Make model non const for safe move ? */
			/* remove potential sbva added variables */
			finalModel.resize(this->beforeSbvaVarCount);
			prs.restoreModel(finalModel);
		}

		if (strat != this) {
			SequentialWorker* winner = (SequentialWorker*)strat;
			winner->solver->printWinningLog();
		} else {
			LOGSTAT("The winner is of type: PRS-PRE");
		}
		/* All this is because I didn't want to change SequentialWorker or make SBVA inherits SolverInterface: big
		 * changes are coming */
		this->joinSbva();
		mutexGlobalEnd.lock();
		condGlobalEnd.notify_all();
		mutexGlobalEnd.unlock();
		LOGDEBUG1("Broadcasted the end");

		for (unsigned int i = 0; i < this->sharers.size(); i++)
			this->sharers[i]->printStats();
	} else { // Else forward the information to the parent strategy
		parent->join(this, res, model);
	}
}

void
PortfolioSBVA::setSolverInterrupt()
{
	this->workersLock.lock();
	for (unsigned int i = 0; i < slaves.size(); i++) {
		slaves[i]->setSolverInterrupt();
	}

	for (auto sbva : sbvas)
		sbva->setSolverInterrupt();

	this->workersLock.unlock();
}

void
PortfolioSBVA::unsetSolverInterrupt()
{
	for (unsigned int i = 0; i < slaves.size(); i++) {
		slaves[i]->unsetSolverInterrupt();
	}

	for (auto sbva : sbvas)
		sbva->unsetSolverInterrupt();
}

void
PortfolioSBVA::waitInterrupt()
{
	for (size_t i = 0; i < slaves.size(); i++) {
		slaves[i]->waitInterrupt();
	}
}

void
PortfolioSBVA::stopSbva()
{
	for (auto sbva : sbvas)
		sbva->setSolverInterrupt();
}

void
PortfolioSBVA::joinSbva()
{
	std::lock_guard<std::mutex> lock(this->sbvaLock);
	/* globalEnding for classical join from solvers */
	if (globalEnding || (++this->sbvaEnded) >= this->sbvas.size())
		this->sbvaSignal.notify_one(); /* only one thread must wait on this */
}
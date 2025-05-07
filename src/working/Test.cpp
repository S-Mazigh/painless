#include "working/Test.hpp"

#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

#include "working/SequentialWorker.hpp"

#include "painless.hpp"

#include <thread>

#include "solvers/SolverFactory.hpp"

#include "preprocessors/StructuredBva.hpp"

#include "containers/ClauseDatabases/ClauseDatabasePerSize.hpp"
#include "sharing/SharingStrategyFactory.hpp"

#include "containers/ClauseDatabases/ClauseDatabaseMallob.hpp"
#include "sharing/GlobalStrategies/MallobSharing.hpp"

#include "preprocessors/GaspiInitializer.hpp"

static void
printClauseStatus(MallobSharing& sharing, ClauseExchangePtr& clause)
{
	std::cout << "Clause " << clause->toString() << ":\n";
	std::cout << "  Exists: " << (sharing.doesClauseExist(clause) ? "Yes" : "No") << "\n";
	std::cout << "  Shared: " << (sharing.isClauseShared(clause) ? "Yes" : "No") << "\n";
	std::cout << "  Can be imported by producer 0: " << (sharing.canConsumerImportClause(clause, 0) ? "Yes" : "No")
			  << "\n";
	std::cout << "  Can be imported by producer 1: " << (sharing.canConsumerImportClause(clause, 1) ? "Yes" : "No")
			  << "\n";
	std::cout << std::endl;
}

Test::Test() {}

Test::~Test() {}

void
Test::solve(const std::vector<int>& cube)
{
	LOG0(">> Test");

	unsigned int varCount, clsCount;
	std::vector<simpleClause> clauses;

	Parsers::parseCNF(__globalParameters__.filename.c_str(), clauses, &varCount);

	int popsize = 20;
	int maxgen = 100;
	float mutrate = 0.5f;
	float crossrate = 0.4f;

	saga::GeneticAlgorithm geneticInit(popsize, (int)varCount, maxgen, mutrate, crossrate,0, clsCount, varCount, clauses);

	auto bestSol = geneticInit.solve();

	auto bestSolAlt = geneticInit.getNthSolution(0);

	assert(bestSol == bestSolAlt);

	std::shared_ptr<SolverInterface> solver;
	SolverFactory::createSolver('k', 'd', solver);

	std::shared_ptr<SolverCdclInterface> cdcl = std::static_pointer_cast<SolverCdclInterface>(solver);

	solver->loadFormula(__globalParameters__.filename.c_str());

	for(unsigned int i = 1; i < bestSol.size(); i++)
		cdcl->setPhase(i, bestSol[i]);
	
	finalResult = cdcl->solve({});

	std::cout << "All tests passed!" << std::endl;

	globalEnding.store(true);
	this->join(this, SatResult::UNKNOWN, {});
}

void
Test::join(WorkingStrategy* strat, SatResult res, const std::vector<int>& model)
{
	mutexGlobalEnd.lock();
	condGlobalEnd.notify_all();
	mutexGlobalEnd.unlock();
}

void
Test::setSolverInterrupt()
{
	for (size_t i = 0; i < slaves.size(); i++) {
		slaves[i]->setSolverInterrupt();
	}
}

void
Test::unsetSolverInterrupt()
{
	for (size_t i = 0; i < slaves.size(); i++) {
		slaves[i]->unsetSolverInterrupt();
	}
}

void
Test::waitInterrupt()
{
	for (size_t i = 0; i < slaves.size(); i++) {
		slaves[i]->waitInterrupt();
	}
}

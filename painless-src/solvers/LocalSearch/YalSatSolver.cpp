#include "YalSatSolver.h"
#include "utils/SatUtils.h"
#include "utils/Parameters.h"
#include "utils/ErrorCodes.h"
#include "utils/System.h"

std::atomic<unsigned> YalSat::yalsatCount(0);

/* For termination callback check */
int yalsat_terminate(void *p_YalSat)
{
    YalSat *cpp_YalSat = (YalSat *)p_YalSat;
    if (cpp_YalSat->terminateSolver)
        return 1;
    else
        return 0;
}

YalSat::YalSat(int _id) : LocalSearchInterface(_id, YalSat::yalsatCount.fetch_add(1), LocalSearchType::YALSAT)
{
    // Yals * yals_new ();
    this->solver = yals_new();
    yals_seterm(this->solver, yalsat_terminate, this);
    this->clausesCount = 0;
}

YalSat::~YalSat()
{
    // void yals_del (Yals *);
    yals_del(this->solver);
    LOGDEBUG1("Yalsat %d deleted!", this->getSolverId());
}

int YalSat::getVariablesCount()
{
    // -1 because of if (idx >= yals->nvars) yals->nvars = idx + 1; in yals_add
    return yals_getnvars(this->solver) - 1;
}

int YalSat::getDivisionVariable()
{
    // TODO: a better division according to the trail (read about divide and conquer)
    return (rand() % getVariablesCount()) + 1;
}

void YalSat::setSolverInterrupt()
{
    if (!this->terminateSolver)
    {
        LOG1("Asked Yalsat %d to terminate", this->getSolverId());
        this->terminateSolver = true;
    }
}

void YalSat::unsetSolverInterrupt()
{
    this->terminateSolver = false;
}

SatResult YalSat::solve(const std::vector<int> &cube)
{
    int res;
    // int yals_sat (Yals *); // do unit propagation as preprocessing
    // set phases using cube
    for (int lit : cube)
    {
        yals_setphase(this->solver, lit);
    }

    res = yals_sat(this->solver);

    this->lsStats.numberFlips = yals_flips(this->solver);
    this->lsStats.numberUnsatClauses = yals_minimum(this->solver);

    LOGSTAT("[YalSat %d] Number of remaining unsats %d / %d, Number of Flips %d.", this->getSolverId(), this->lsStats.numberUnsatClauses, this->clausesCount, this->lsStats.numberFlips);

    if (static_cast<int>(SatResult::SAT) != res)
    {
        return SatResult::UNKNOWN;
    }

    return SatResult::SAT;
}

void YalSat::setPhase(const unsigned var, const bool phase)
{
    yals_setphase(this->solver, (phase) ? var : -var);
}

void YalSat::addClause(std::shared_ptr<ClauseExchange> clause)
{
    for (int lit : clause->lits)
    {
        yals_add(this->solver, lit);
    }
    yals_add(this->solver, 0);
}

void YalSat::addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
    for (auto clause : clauses)
    {
        addClause(clause);
    }
}

void YalSat::addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars)
{
    if (clauses.size() > 33 * MILLION)
    {
        LOGERROR("The number of clauses %u is too high for yalsat!", clauses.size());
        exit(PERR_NOT_SUPPORTED);
    }
    for (auto clause : clauses)
    {
        for (int lit : clause)
        {
            yals_add(this->solver, lit);
        }
        yals_add(this->solver, 0);
    }

    this->setInitialized(true);

    this->clausesCount = clauses.size();
    LOG1("Yalsat %d loaded all the %d clauses with %u variables", this->getSolverId(), this->clausesCount, nbVars);
}

void YalSat::loadFormula(const char *filename)
{
    unsigned parsedVarCount;
    std::vector<std::vector<int>> clauses;
    parseFormula(filename, clauses, &parsedVarCount);
    this->addInitialClauses(clauses, parsedVarCount);
}

std::vector<int> YalSat::getModel()
{
    std::vector<int> model;
    unsigned varCount = this->getVariablesCount();

    for (int i = 1; i <= varCount; i++)
    {
        /* yals_deref returns the best value */
        model.emplace_back(yals_deref(this->solver, i) > 0 ? i : -i);
    }

    return model;
}

void YalSat::printStatistics()
{
    yals_stats(this->solver);
}

void YalSat::printParameters()
{
    // void yals_usage (Yals *); to be used in printHelp
    yals_showopts(this->solver);
}

void YalSat::diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist)
{
    if (!getVariablesCount())
    {
        LOGERROR("Please call diversify after initializing the solver and adding the problem's clauses");
        exit(PERR_NOT_SUPPORTED);
    }
    /* This is a totally random diversification */
    int maxNoise = uniform_dist.max();

    yals_setflipslimit(this->solver, Parameters::getIntParam("ls-flips", -1));

    // Seed the random number generator for the solver
    yals_srand(this->solver, this->getSolverId());

    // Set options that influence the local search algorithm with random values

    // Walk probability: controls the probability of random walks during the local search
    yals_setopt(this->solver, "walk", uniform_dist(rng_engine) > (maxNoise / 2));
    yals_setopt(this->solver, "walkprobability", (uniform_dist(rng_engine) * 100) / maxNoise);

    // Eager: controls whether to eagerly pick minimum break literals
    yals_setopt(this->solver, "eager", uniform_dist(rng_engine) % 2);

    // Unfair frequency: controls how often unfair picking strategy is used
    yals_setopt(this->solver, "unfairfreq", (uniform_dist(rng_engine) * 100) / maxNoise);

    // Reluctant: controls the reluctant doubling of the restart interval
    yals_setopt(this->solver, "reluctant", uniform_dist(rng_engine) % 2);

    // Toggle uniform: toggles the uniform strategy
    yals_setopt(this->solver, "toggleuniform", uniform_dist(rng_engine) % 2);

    // Dynamic break values (using critical literals)
    yals_setopt(this->solver, "crit", uniform_dist(rng_engine) % 2);

    // Geometric picking frequency: determines how often the geometric picking strategy is used
    yals_setopt(this->solver, "geomfreq", (uniform_dist(rng_engine) * 100) / maxNoise);

    // Polarity: controls the polarity of literals
    yals_setopt(this->solver, "pol", uniform_dist(rng_engine) % 3 - 1);

    // Clause picking strategy for uniform formulas
    yals_setopt(this->solver, "unipick", uniform_dist(rng_engine) % 6 - 1);

    // CB (confidence-based) value in percent
    yals_setopt(this->solver, "cb", (uniform_dist(rng_engine) * 100) / maxNoise);

    // Fixed strategy frequency: determines how often the fixed strategy is applied
    yals_setopt(this->solver, "fixed", uniform_dist(rng_engine) % 5);

    // Restart interval: basic (inner) restart interval
    yals_setopt(this->solver, "restart", uniform_dist(rng_engine) + 100000);

    // Outer restart interval factor: enables the outer restart interval factor
    yals_setopt(this->solver, "restartouter", uniform_dist(rng_engine) % 2);
    yals_setopt(this->solver, "restartouterfactor", uniform_dist(rng_engine) + 50);

    // Critical literals setting
    yals_setopt(this->solver, "crit", uniform_dist(rng_engine) % 2);

    // Correct CB value depending on maximum length
    yals_setopt(this->solver, "correct", uniform_dist(rng_engine) % 2);

    /* Restarts */
    yals_setopt(this->solver, "cached", 0);
    int noise = uniform_dist(rng_engine);
    if (noise < maxNoise / 3)
    {
        yals_setopt(this->solver, "best", 1);
    }
    else if (noise > maxNoise > 2 * (maxNoise / 3))
    {
        yals_setopt(this->solver, "cacheduni", 1);
    }
    else
    {
        yals_setopt(this->solver, "cached", 1);
    }

    LOG1("Diversification of Yalsat(%d,%u) done", this->getSolverId(), this->getSolverTypeId());
}
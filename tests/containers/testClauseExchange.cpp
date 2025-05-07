#include "../catch.hpp"
#include "src/containers/ClauseExchange.h"

TEST_CASE("ClauseExchange basic functionality", "[ClauseExchange]") {
    SECTION("Creation and basic properties") {
        auto ce = ClauseExchange::create(3, 2, 1);
        
        REQUIRE(ce->size == 3);
        REQUIRE(ce->lbd == 2);
        REQUIRE(ce->from == 1);
        REQUIRE(ce->refCounter == 1);
    }

    SECTION("Creating from vector") {
        std::vector<int> literals = {1, -2, 3};
        auto ce = ClauseExchange::create(literals, 2, 1);

        REQUIRE(ce->size == 3);
        REQUIRE(ce->lbd == 2);
        REQUIRE(ce->from == 1);
        REQUIRE(ce->refCounter == 1);

        REQUIRE(ce->lits[0] == 1);
        REQUIRE(ce->lits[1] == -2);
        REQUIRE(ce->lits[2] == 3);
    }

    SECTION("Creating from pointers") {
        int literals[] = {1, -2, 3, 4};
        auto ce = ClauseExchange::create(literals, literals + 4, 2, 1);

        REQUIRE(ce->size == 4);
        REQUIRE(ce->lbd == 2);
        REQUIRE(ce->from == 1);
        REQUIRE(ce->refCounter == 1);

        REQUIRE(ce->lits[0] == 1);
        REQUIRE(ce->lits[1] == -2);
        REQUIRE(ce->lits[2] == 3);
        REQUIRE(ce->lits[3] == 4);

        // Check that the data was copied, not just referenced
        literals[0] = 10;
        REQUIRE(ce->lits[0] == 1);
    }

    SECTION("Accessing elements") {
        std::vector<int> literals = {1, -2, 3};
        auto ce = ClauseExchange::create(literals);

        REQUIRE(ce->lits[0] == 1);
        REQUIRE(ce->lits[1] == -2);
        REQUIRE(ce->lits[2] == 3);

        ce->lits[1] = 2;
        REQUIRE(ce->lits[1] == 2);
    }

    SECTION("Iterators") {
        std::vector<int> literals = {1, -2, 3};
        auto ce = ClauseExchange::create(literals);

        REQUIRE(*ce->begin() == 1);
        REQUIRE(*(ce->end() - 1) == 3);

        int sum = 0;
        for (const auto& lit : *ce) {
            sum += std::abs(lit);
        }
        REQUIRE(sum == 6);
    }

    SECTION("Sorting") {
        std::vector<int> literals = {3, -1, 2};
        auto ce = ClauseExchange::create(literals);

        ce->sortLiterals();
        REQUIRE(ce->lits[0] == -1);
        REQUIRE(ce->lits[1] == 2);
        REQUIRE(ce->lits[2] == 3);

        ce->sortLiteralsDescending();
        REQUIRE(ce->lits[0] == 3);
        REQUIRE(ce->lits[1] == 2);
        REQUIRE(ce->lits[2] == -1);
    }

    SECTION("toString") {
        std::vector<int> literals = {1, -2, 3};
        auto ce = ClauseExchange::create(literals, 2, 1);

        std::string str = ce->toString();
        REQUIRE(str.find("size: 3") != std::string::npos);
        REQUIRE(str.find("lbd: 2") != std::string::npos);
        REQUIRE(str.find("from: 1") != std::string::npos);
        REQUIRE(str.find("lits: {1, -2, 3}") != std::string::npos);
    }

    SECTION("Reference counting") {
        auto ce1 = ClauseExchange::create(3);
        REQUIRE(ce1->refCounter == 1);

        {
            auto ce2 = ce1;
            REQUIRE(ce1->refCounter == 2);
            REQUIRE(ce2->refCounter == 2);
        }

        REQUIRE(ce1->refCounter == 1);

        auto raw_ptr = ce1->toRawPtr();
        REQUIRE(ce1->refCounter == 2);

        auto ce3 = ClauseExchange::fromRawPtr(raw_ptr);
        REQUIRE(ce1->refCounter == 3);
        REQUIRE(ce3->refCounter == 3);
    }
}
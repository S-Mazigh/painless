#include "tests/catch.hpp"
#include "src/containers/Bitset.hpp"

TEST_CASE("Bitset Construction and Basic Operations", "[bitset]") {
    SECTION("Default construction") {
        Bitset b(64);
        REQUIRE(b.size() == 64);
        for (size_t i = 0; i < 64; ++i) {
            REQUIRE(b[i] == false);
        }
    }

    SECTION("Construction with default value true") {
        Bitset b(64, true);
        REQUIRE(b.size() == 64);
        for (size_t i = 0; i < 64; ++i) {
            REQUIRE(b[i] == true);
        }
    }

    SECTION("Non-multiple of 64 size") {
        Bitset b(100);
        REQUIRE(b.size() == 100);
        for (size_t i = 0; i < 100; ++i) {
            REQUIRE(b[i] == false);
        }
    }

    SECTION("Setting and getting bits") {
        Bitset b(100);
        b.set(50, true);
        b.set(75, true);
        REQUIRE(b[50] == true);
        REQUIRE(b[75] == true);
        REQUIRE(b[0] == false);
        REQUIRE(b[99] == false);
    }

    SECTION("Clearing bits") {
        Bitset b(100, true);
        b.clear();
        for (size_t i = 0; i < 100; ++i) {
            REQUIRE(b[i] == false);
        }
    }

    SECTION("Resizing smaller") {
        Bitset b(100, true);
        b.resize(50);
        REQUIRE(b.size() == 50);
        for (size_t i = 0; i < 50; ++i) {
            REQUIRE(b[i] == true);
        }
    }

    SECTION("Resizing larger") {
        Bitset b(50, true);
        b.resize(100);
        REQUIRE(b.size() == 100);
        for (size_t i = 0; i < 50; ++i) {
            REQUIRE(b[i] == true);
        }
        for (size_t i = 50; i < 100; ++i) {
            REQUIRE(b[i] == false);
        }
    }
}

TEST_CASE("Bitset Merge Operations", "[bitset]") {
    SECTION("OR merge") {
        Bitset b1(100);
        Bitset b2(100);
        Bitset b3(100);

        b1.set(25, true);
        b1.set(75, true);
        b2.set(50, true);
        b3.set(75, true);

        std::vector<Bitset> others = {b2, b3};
        b1.merge_or(others);

        REQUIRE(b1[25] == true);
        REQUIRE(b1[50] == true);
        REQUIRE(b1[75] == true);
        REQUIRE(b1[0] == false);
        REQUIRE(b1[99] == false);
    }

    SECTION("AND merge") {
        Bitset b1(100, true);
        Bitset b2(100, true);
        Bitset b3(100, true);

        b1.set(25, false);
        b2.set(50, false);
        b3.set(75, false);

        std::vector<Bitset> others = {b2, b3};
        b1.merge_and(others);

        REQUIRE(b1[0] == true);
        REQUIRE(b1[25] == false);
        REQUIRE(b1[50] == false);
        REQUIRE(b1[75] == false);
        REQUIRE(b1[99] == true);
    }

    SECTION("Custom XOR merge") {
        Bitset b1(100);
        Bitset b2(100);
        Bitset b3(100);

        b1.set(25, true);
        b1.set(75, true);
        b2.set(25, true);
        b2.set(50, true);
        b3.set(50, true);
        b3.set(75, true);

        std::vector<Bitset> others = {b2, b3};
        b1.merge(others, std::bit_xor<unsigned long long>());

        REQUIRE(b1[25] == false);
        REQUIRE(b1[50] == false);
        REQUIRE(b1[75] == false);
        REQUIRE(b1[0] == false);
        REQUIRE(b1[99] == false);
    }
}

TEST_CASE("Bitset Edge Cases", "[bitset]") {
    SECTION("Empty bitset") {
        Bitset b(0);
        REQUIRE(b.size() == 0);
    }

    SECTION("Single bit bitset") {
        Bitset b(1);
        REQUIRE(b.size() == 1);
        REQUIRE(b[0] == false);
        b.set(0, true);
        REQUIRE(b[0] == true);
    }

    SECTION("Bitset of size 63 (just under a full block)") {
        Bitset b(63, true);
        REQUIRE(b.size() == 63);
        for (size_t i = 0; i < 63; ++i) {
            REQUIRE(b[i] == true);
        }
    }

    SECTION("Bitset of size 65 (just over a full block)") {
        Bitset b(65);
        b.set(64, true);
        REQUIRE(b.size() == 65);
        REQUIRE(b[64] == true);
        for (size_t i = 0; i < 64; ++i) {
            REQUIRE(b[i] == false);
        }
    }
}

TEST_CASE("Bitset Performance", "[bitset][.slow]") {
    SECTION("Large bitset operations") {
        const size_t size = 1000000;
        Bitset b(size);

        // Set every 1000th bit
        for (size_t i = 0; i < size; i += 1000) {
            b.set(i, true);
        }

        // Check setting worked correctly
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(b[i] == (i % 1000 == 0));
        }

        // Perform a large merge operation
        std::vector<Bitset> others;
        for (int i = 0; i < 10; ++i) {
            Bitset other(size);
            for (size_t j = i; j < size; j += 10) {
                other.set(j, true);
            }
            others.push_back(std::move(other));
        }

        b.merge_or(others);

        // Check merge result
        for (size_t i = 0; i < size; ++i) {
            REQUIRE(b[i] == (i % 10 == 0 || i % 1000 == 0));
        }
    }
}
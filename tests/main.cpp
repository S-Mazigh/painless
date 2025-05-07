#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// This file should be left empty aside from the above two lines.
// Catch2 will provide a main() function automatically.

// You can add any global setup/teardown here if needed:

/*
CATCH_CONFIG_MAIN tells Catch2 to generate a main() function automatically.
This should typically be done in only one source file.

If you need any global setup or teardown, you can use the following:

CATCH_TEST_CASE_METHOD(GlobalFixture, "Global setup and teardown", "[]") {
    // This test case will run first and last
}

class GlobalFixture {
public:
    GlobalFixture()  {  Global setup code }
    ~GlobalFixture() {  Global teardown code }
};
*/
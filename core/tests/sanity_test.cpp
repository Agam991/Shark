#include "shark/test.hpp"
#include "shark/version.hpp"

#include <string>

// Phase 0 smoke tests: prove the build + test harness actually work end-to-end.
// These get fleshed out with real engine assertions as Phase 1+ lands.

TEST("harness sanity: arithmetic") {
    CHECK(2 + 2 == 4);
    CHECK_EQ(6 * 7, 42);
}

TEST("version metadata is wired up") {
    CHECK_EQ(shark::kVersionMajor, 0);
    CHECK(std::string(shark::kVersionString) == "0.1.0");
    CHECK(std::string(shark::kEngineName) == "Shark");
}

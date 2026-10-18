#pragma once

// A tiny, zero-dependency test harness.
//
// Why not doctest/Catch2? Phase 0 stays offline-friendly and dependency-free.
// If/when we need fixtures, tags, or richer output we can swap this out for
// doctest via CMake FetchContent without changing test call sites much.
//
// Usage:
//   #include "shark/test.hpp"
//   TEST("addition works") { CHECK(2 + 2 == 4); }
//   int main() { return shark::test::run_all(); }

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace shark::test {

struct Case {
    std::string name;
    std::function<void(int&)> fn;  // fn receives a failure counter to bump
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(std::string name, std::function<void(int&)> fn) {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

inline int run_all() {
    int failed_cases = 0;
    for (auto& c : registry()) {
        int local_failures = 0;
        c.fn(local_failures);
        if (local_failures == 0) {
            std::printf("[ PASS ] %s\n", c.name.c_str());
        } else {
            std::printf("[ FAIL ] %s (%d check(s) failed)\n",
                        c.name.c_str(), local_failures);
            ++failed_cases;
        }
    }
    std::printf("\n%zu case(s), %d failed.\n", registry().size(), failed_cases);
    return failed_cases == 0 ? 0 : 1;
}

}  // namespace shark::test

// ---- Macros -----------------------------------------------------------------
#define SHK_CONCAT_INNER(a, b) a##b
#define SHK_CONCAT(a, b) SHK_CONCAT_INNER(a, b)

#define TEST(name_literal)                                                     \
    static void SHK_CONCAT(shk_test_fn_, __LINE__)(int&);                        \
    static ::shark::test::Registrar SHK_CONCAT(shk_test_reg_, __LINE__)(     \
        name_literal, SHK_CONCAT(shk_test_fn_, __LINE__));                       \
    static void SHK_CONCAT(shk_test_fn_, __LINE__)(int& shk_failures)

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::printf("    CHECK failed: %s  (%s:%d)\n", #expr,              \
                        __FILE__, __LINE__);                                   \
            ++shk_failures;                                                     \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        if (!((a) == (b))) {                                                   \
            std::printf("    CHECK_EQ failed: %s == %s  (%s:%d)\n", #a, #b,    \
                        __FILE__, __LINE__);                                   \
            ++shk_failures;                                                     \
        }                                                                      \
    } while (0)

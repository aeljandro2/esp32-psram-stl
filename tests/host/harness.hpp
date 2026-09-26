// A tiny test harness: no dependencies, readable output, non-zero exit on failure.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace harness {

struct test_case {
    const char* name;
    std::function<void()> body;
};

inline std::vector<test_case>& registry() {
    static std::vector<test_case> tests;
    return tests;
}

inline int& failures_in_current_test() {
    static int n = 0;
    return n;
}

struct registrar {
    registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

inline void fail(const char* file, int line, const std::string& what) {
    ++failures_in_current_test();
    std::printf("    %s:%d: %s\n", file, line, what.c_str());
}

}  // namespace harness

#define HARNESS_CONCAT2(a, b) a##b
#define HARNESS_CONCAT(a, b) HARNESS_CONCAT2(a, b)

#define TEST(name)                                                                              \
    static void HARNESS_CONCAT(test_, name)();                                                  \
    static harness::registrar HARNESS_CONCAT(registrar_, name)(#name, HARNESS_CONCAT(test_, name)); \
    static void HARNESS_CONCAT(test_, name)()

#define CHECK(expr)                                                  \
    do {                                                             \
        if (!(expr)) harness::fail(__FILE__, __LINE__, "CHECK(" #expr ")"); \
    } while (0)

#define CHECK_EQ(a, b)                                                                                     \
    do {                                                                                                   \
        const auto& va_ = (a);                                                                             \
        const auto& vb_ = (b);                                                                             \
        if (!(va_ == vb_))                                                                                 \
            harness::fail(__FILE__, __LINE__,                                                              \
                          std::string("CHECK_EQ(" #a ", " #b "): ") + std::to_string(va_) + " != " + std::to_string(vb_)); \
    } while (0)

#define CHECK_THROWS(expr, type)                                                  \
    do {                                                                          \
        bool thrown_ = false;                                                     \
        try {                                                                     \
            (void)(expr);                                                         \
        } catch (const type&) {                                                   \
            thrown_ = true;                                                       \
        }                                                                         \
        if (!thrown_) harness::fail(__FILE__, __LINE__, "expected " #type " from " #expr); \
    } while (0)

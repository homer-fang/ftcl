#include "commands.hpp"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>

#define TEST(name) \
    void test_##name() { \
        std::cout << "Testing " #name "..." << std::endl;

#define ASSERT_TRUE(condition, message) \
    if (!(condition)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  [OK] " << message << std::endl; \
    }

#define ASSERT_EQ(actual, expected, message) \
    if ((actual) != (expected)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::cerr << "  expected: [" << expected << "]" << std::endl; \
        std::cerr << "  actual:   [" << actual << "]" << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  [OK] " << message << std::endl; \
    }

#define END_TEST \
    std::cout << "  [OK] Test passed!" << std::endl << std::endl; \
    }

using namespace ftcl;

static Value eval_ok(Interp& interp, const std::string& script, const std::string& label) {
    auto result = interp.eval(script);
    if (!result.has_value()) {
        std::cerr << "FAILED: " << label << std::endl;
        std::cerr << "  error: " << result.error().value().as_string() << std::endl;
        std::exit(1);
    }
    return *result;
}

TEST(time_output_and_count_semantics)
    auto interp = new_interp_with_stdlib();

    const std::string out = eval_ok(interp, "time {set t 1}", "time basic").as_string();
    const std::string suffix = " nanoseconds per iteration";
    ASSERT_TRUE(out.size() > suffix.size(), "time output should include numeric prefix");
    ASSERT_TRUE(out.rfind(suffix) == out.size() - suffix.size(), "time output should use nanoseconds suffix");
    ASSERT_TRUE(std::isdigit(static_cast<unsigned char>(out[0])) != 0, "time output should begin with a digit");

    ASSERT_EQ(eval_ok(interp, "time {set t 1} 0", "time zero count").as_string(),
              "0 nanoseconds per iteration",
              "time with count=0 should execute zero iterations and return 0 average");

    ASSERT_EQ(eval_ok(interp, "time {set t 1} -3", "time negative count").as_string(),
              "0 nanoseconds per iteration",
              "time with negative count should execute zero iterations and return 0 average");
END_TEST

TEST(time_error_propagation)
    auto interp = new_interp_with_stdlib();

    auto err = interp.eval("time {error boom} 2");
    ASSERT_TRUE(!err.has_value(), "time should propagate body errors");
    ASSERT_EQ(err.error().value().as_string(), "boom", "time should return original error message");
END_TEST

int main() {
    std::cout << "=== Testing Time Semantics ===" << std::endl << std::endl;

    test_time_output_and_count_semantics();
    test_time_error_propagation();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

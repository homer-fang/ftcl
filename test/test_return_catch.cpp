#include "commands.hpp"

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

static Exception eval_err(Interp& interp, const std::string& script, const std::string& label) {
    auto result = interp.eval(script);
    if (result.has_value()) {
        std::cerr << "FAILED: " << label << std::endl;
        std::cerr << "  expected error, got value: " << result->as_string() << std::endl;
        std::exit(1);
    }
    return result.error();
}

TEST(top_level_return_break_continue_translation)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "return 7", "top-level return").as_string(),
              "7",
              "top-level `return` should become a normal value");

    auto break_err = eval_err(interp, "break", "top-level break");
    ASSERT_TRUE(break_err.value().as_string().find("invoked \"break\" outside of a loop") != std::string::npos,
                "top-level break should be translated to standard error");

    auto continue_err = eval_err(interp, "continue", "top-level continue");
    ASSERT_TRUE(continue_err.value().as_string().find("invoked \"continue\" outside of a loop") != std::string::npos,
                "top-level continue should be translated to standard error");
END_TEST

TEST(catch_return_option_semantics)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "catch {return 9}", "catch return code").as_string(),
              "2",
              "catch should return 2 for Return exception");

    ASSERT_EQ(eval_ok(interp,
                      "catch {return 9} r o; list $r [dict get $o -code] [dict get $o -level]",
                      "catch options for default return")
                  .as_string(),
              "9 0 1",
              "return options should expose next code=0 and level=1 for plain return");

    ASSERT_EQ(eval_ok(interp,
                      "catch {return -code break x} r o; list [set r] [dict get $o -code] [dict get $o -level]",
                      "catch options for return -code break")
                  .as_string(),
              "x 3 1",
              "return options should expose next code=3 and level=1 for return -code break");

    ASSERT_EQ(eval_ok(interp,
                      "catch {return -level 0 y} r o; list [set r] [dict get $o -code] [dict get $o -level]",
                      "catch options for return -level 0")
                  .as_string(),
              "y 0 0",
              "return -level 0 should be treated as normal success");
END_TEST

TEST(catch_error_option_semantics)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "catch {error boom} r o; list [set r] [dict get $o -code] [dict get $o -level]",
                      "catch options for error")
                  .as_string(),
              "boom 1 0",
              "error options should expose code=1 and level=0");

    ASSERT_EQ(eval_ok(interp,
                      "catch {error boom} r o; list [dict exists $o -errorcode] [dict exists $o -errorinfo]",
                      "catch error option presence")
                  .as_string(),
              "1 1",
              "error options should include -errorcode and -errorinfo");

    ASSERT_EQ(eval_ok(interp,
                      "catch {return -code error -errorcode A -errorinfo B -level 0 x} r o; "
                      "list [set r] [set errorCode] [expr {$errorInfo eq [dict get $o -errorinfo]}]",
                      "catch should populate errorCode/errorInfo for immediate errors")
                  .as_string(),
              "x A 1",
              "catch should update errorCode/errorInfo in the current scope for error results");
END_TEST

int main() {
    std::cout << "=== Testing Return/Catch Semantics ===" << std::endl << std::endl;

    test_top_level_return_break_continue_translation();
    test_catch_return_option_semantics();
    test_catch_error_option_semantics();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

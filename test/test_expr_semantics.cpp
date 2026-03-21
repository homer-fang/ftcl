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

TEST(expr_reads_scalar_variable_values)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "set i 0; list [expr {$i < 5}] [expr {$i > 5}]", "scalar expr at i=0").as_string(),
              "1 0",
              "expr should use scalar variable value in relational operators");

    ASSERT_EQ(eval_ok(interp, "set i 9; list [expr {$i < 5}] [expr {$i > 5}]", "scalar expr at i=9").as_string(),
              "0 1",
              "expr results should change when variable value changes");
END_TEST

TEST(expr_reads_array_elements)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "array set a {x 2}; expr {$a(x) + 3}", "array element in expr").as_string(),
              "5",
              "expr should resolve array-element references");
END_TEST

TEST(expr_command_subst_break_continue_are_normalized)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "catch {for {} {[break]} {} {}} msg; set msg", "break in expr command substitution").as_string(),
              "invoked \"break\" outside of a loop",
              "break from expr command substitution should become standard outside-loop error");

    ASSERT_EQ(eval_ok(interp,
                      "catch {for {} {[continue]} {} {}} msg; set msg",
                      "continue in expr command substitution")
                  .as_string(),
              "invoked \"continue\" outside of a loop",
              "continue from expr command substitution should become standard outside-loop error");
END_TEST

TEST(expr_bitwise_and_ternary_short_circuit)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set a 0; set b 0; "
                      "set bit [expr {6 & 3 | 8 ^ 1}]; "
                      "set t1 [expr {1 ? [set a 11] : [set b 22]}]; "
                      "set t2 [expr {0 ? [set a 33] : [set b 22]}]; "
                      "list $bit $t1 $t2 $a $b",
                      "bitwise precedence and ternary short-circuit")
                  .as_string(),
              "11 11 22 11 22",
              "bitwise operators and ternary should match ftcl semantics");

    ASSERT_EQ(eval_ok(interp, "catch {expr {1.0 & 1}} msg; set msg", "bitwise float operand error").as_string(),
              "can't use floating-point value as operand of \"&\"",
              "bitwise operators should reject floating-point operands");
END_TEST

TEST(expr_logical_short_circuit_skips_command_substitution)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "list [expr {0 && [missing_cmd]}] [expr {1 || [missing_cmd]}]",
                      "logical short-circuit should skip missing command")
                  .as_string(),
              "0 1",
              "logical operators should short-circuit command substitution");

    ASSERT_EQ(eval_ok(interp,
                      "set hits 0; expr {0 && [set hits 1]}; expr {1 || [set hits 2]}; set hits",
                      "logical short-circuit side effects")
                  .as_string(),
              "0",
              "short-circuiting should avoid side effects in skipped branch");
END_TEST

TEST(expr_integer_div_mod_and_overflow_semantics)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "list [expr {1/-2}] [expr {-3/2}] [expr {-3%2}] [expr {2*(-3/2) + (-3%2)}]",
                      "integer division and remainder semantics")
                  .as_string(),
              "0 -1 -1 -3",
              "integer division/remainder should be truncating and self-consistent");

    ASSERT_EQ(eval_ok(interp,
                      "catch {expr {9223372036854775807 + 1}} msg; set msg",
                      "sum overflow message")
                  .as_string(),
              "integer overflow",
              "integer addition overflow should be detected");

    ASSERT_EQ(eval_ok(interp,
                      "catch {expr {(-9223372036854775807 - 1) / -1}} msg; set msg",
                      "division overflow message")
                  .as_string(),
              "integer overflow",
              "integer division overflow should be detected");
END_TEST

int main() {
    std::cout << "=== Testing Expr Semantics ===" << std::endl << std::endl;

    test_expr_reads_scalar_variable_values();
    test_expr_reads_array_elements();
    test_expr_command_subst_break_continue_are_normalized();
    test_expr_bitwise_and_ternary_short_circuit();
    test_expr_logical_short_circuit_skips_command_substitution();
    test_expr_integer_div_mod_and_overflow_semantics();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

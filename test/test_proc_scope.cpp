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

TEST(proc_varargs_and_defaults)
    auto interp = new_interp_with_stdlib();

    ASSERT_TRUE(eval_ok(interp, "proc p {args} {join $args ,}", "define varargs proc").as_string().empty(),
                "proc p should be defined");
    ASSERT_EQ(eval_ok(interp, "p a b c", "call varargs proc").as_string(),
              "a,b,c",
              "trailing args formal should collect all remaining arguments");

    ASSERT_TRUE(eval_ok(interp, "proc q {a {b 2}} {list $a $b}", "define default proc").as_string().empty(),
                "proc q should be defined");
    ASSERT_EQ(eval_ok(interp, "q 1", "default argument").as_string(),
              "1 2",
              "missing optional argument should use default");
    ASSERT_EQ(eval_ok(interp, "q 1 3", "override default argument").as_string(),
              "1 3",
              "provided optional argument should override default");

    ASSERT_EQ(eval_ok(interp, "catch {q} msg; set msg", "wrong args message").as_string(),
              "wrong # args: should be \"q a ?b?\"",
              "wrong-args message should include required/optional formals");
END_TEST

TEST(args_only_special_when_last)
    auto interp = new_interp_with_stdlib();

    ASSERT_TRUE(eval_ok(interp, "proc r {args x} {list $args $x}", "define non-varargs args").as_string().empty(),
                "proc r should be defined");
    ASSERT_EQ(eval_ok(interp, "r A B", "call proc with literal args formal").as_string(),
              "A B",
              "\"args\" should be treated as normal name when not last");
END_TEST

TEST(proc_break_continue_are_errors)
    auto interp = new_interp_with_stdlib();

    ASSERT_TRUE(eval_ok(interp, "proc pb {} {break}", "define break proc").as_string().empty(),
                "proc pb should be defined");
    ASSERT_EQ(eval_ok(interp, "catch {pb} msg", "catch break in proc").as_string(),
              "1",
              "break in proc should be error code 1");
    ASSERT_TRUE(eval_ok(interp, "set msg", "break message").as_string().find("invoked \"break\" outside of a loop") !=
                    std::string::npos,
                "break in proc should produce standard break-outside-loop message");

    ASSERT_TRUE(eval_ok(interp, "proc pc {} {continue}", "define continue proc").as_string().empty(),
                "proc pc should be defined");
    ASSERT_EQ(eval_ok(interp, "catch {pc} msg", "catch continue in proc").as_string(),
              "1",
              "continue in proc should be error code 1");
    ASSERT_TRUE(eval_ok(interp, "set msg", "continue message")
                        .as_string()
                        .find("invoked \"continue\" outside of a loop") != std::string::npos,
                "continue in proc should produce standard continue-outside-loop message");
END_TEST

int main() {
    std::cout << "=== Testing Proc/Scope Semantics ===" << std::endl << std::endl;

    test_proc_varargs_and_defaults();
    test_args_only_special_when_last();
    test_proc_break_continue_are_errors();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

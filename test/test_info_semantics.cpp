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

TEST(info_proc_and_var_introspection)
    auto interp = new_interp_with_stdlib();

    ASSERT_TRUE(eval_ok(interp, "proc p {a {b 2}} {list $a $b}", "define proc").as_string().empty(),
                "proc definition should succeed");

    ASSERT_EQ(eval_ok(interp, "info args p", "info args").as_string(),
              "a b",
              "info args should return formal parameter names");
    ASSERT_EQ(eval_ok(interp, "info body p", "info body").as_string(),
              "list $a $b",
              "info body should return original body script");

    ASSERT_EQ(eval_ok(interp, "info default p b dv; list [set dv] [info default p a dv]", "info default").as_string(),
              "2 0",
              "info default should set var and return 1/0 accordingly");

    ASSERT_EQ(eval_ok(interp, "set x 1; list [info exists x] [info exists y]", "info exists").as_string(),
              "1 0",
              "info exists should reflect variable existence");
END_TEST

TEST(info_complete_and_cmdtype)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "list [info complete {set a [list b]}] [info complete {set a [list b}]", "info complete")
                  .as_string(),
              "1 0",
              "info complete should detect complete/incomplete scripts");

    ASSERT_TRUE(eval_ok(interp, "proc p {} {}", "define proc").as_string().empty(), "proc definition should succeed");
    ASSERT_EQ(eval_ok(interp, "list [info cmdtype set] [info cmdtype p]", "info cmdtype").as_string(),
              "native proc",
              "info cmdtype should distinguish native command and proc");

    auto unknown = interp.eval("info cmdtype __missing_cmd__");
    ASSERT_TRUE(!unknown.has_value(), "unknown command for info cmdtype should fail");
    ASSERT_TRUE(unknown.error().value().as_string().find("isn't a command") != std::string::npos,
                "info cmdtype error should match Tcl/ftcl wording");
END_TEST

TEST(info_error_and_local_scope_details)
    auto interp = new_interp_with_stdlib();

    auto missing_sub = interp.eval("info");
    ASSERT_TRUE(!missing_sub.has_value(), "info without subcommand should fail");
    ASSERT_EQ(missing_sub.error().value().as_string(),
              "wrong # args: should be \"info subcommand ?arg ...?\"",
              "info missing-subcommand error should match Tcl/ftcl wording");

    ASSERT_EQ(eval_ok(interp, "info locals", "info locals at top-level").as_string(),
              "",
              "info locals should be empty at top level");

    eval_ok(interp, "proc p {a {b 2}} {list $a $b}", "define proc for info default");
    auto missing_arg = interp.eval("info default p c v");
    ASSERT_TRUE(!missing_arg.has_value(), "info default on missing arg should fail");
    ASSERT_TRUE(missing_arg.error().value().as_string().find("doesn't have an argument") != std::string::npos,
                "info default should report missing procedure argument");
END_TEST

int main() {
    std::cout << "=== Testing Info Semantics ===" << std::endl << std::endl;

    test_info_proc_and_var_introspection();
    test_info_complete_and_cmdtype();
    test_info_error_and_local_scope_details();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

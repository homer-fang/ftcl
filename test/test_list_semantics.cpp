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

TEST(lindex_index_list_and_nested_indexing)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "lindex {a b c} {1}", "single-arg index list").as_string(),
              "b",
              "when only one index argument is present, it should be parsed as an index list");

    ASSERT_EQ(eval_ok(interp, "lindex {{a b} {c d}} {1 0}", "single-arg nested indices").as_string(),
              "c",
              "single index-list argument should support nested list indexing");

    ASSERT_EQ(eval_ok(interp, "lindex {{a b} {c d}} 1 1", "multi-arg nested indices").as_string(),
              "d",
              "multiple index arguments should support nested list indexing");
END_TEST

TEST(lindex_out_of_range_and_parse_errors)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "lindex {a b} 7", "out of range").as_string(),
              "",
              "out-of-range index should return empty string");

    auto err = interp.eval("lindex {a b c} {x}");
    ASSERT_TRUE(!err.has_value(), "non-integer index should fail");
    ASSERT_TRUE(err.error().value().as_string().find("expected integer") != std::string::npos,
                "error should mention integer expectation");
END_TEST

TEST(lappend_quoting_and_list_parse_errors)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "list [lappend x 1 2 abc \"long string\"] $x",
                      "lappend should preserve nested list element text")
                  .as_string(),
              "{1 2 abc {long string}} {1 2 abc {long string}}",
              "list encoding should not inject spurious backslashes for balanced braces");

    ASSERT_EQ(eval_ok(interp, "set x {}; lappend x \\{\\  abc", "lappend unmatched brace data").as_string(),
              "\\{\\  abc",
              "unbalanced brace content should be backslash-quoted in canonical list output");

    ASSERT_EQ(eval_ok(interp,
                      "set x \\\"; catch {lappend x} msg; set msg",
                      "lappend unmatched quote parse error")
                  .as_string(),
              "unmatched open quote in list",
              "list parser error text should match Tcl/ftcl wording for open quotes");
END_TEST

int main() {
    std::cout << "=== Testing List Semantics ===" << std::endl << std::endl;

    test_lindex_index_list_and_nested_indexing();
    test_lindex_out_of_range_and_parse_errors();
    test_lappend_quoting_and_list_parse_errors();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

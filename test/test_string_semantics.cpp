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

TEST(string_char_index_semantics)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "string length a中b", "unicode char length").as_string(),
              "3",
              "string length should count Unicode code points, not bytes");

    ASSERT_EQ(eval_ok(interp, "string first 中 a中b中", "string first").as_string(),
              "1",
              "string first should return character index");
    ASSERT_EQ(eval_ok(interp, "string first 中 a中b中 2", "string first with start").as_string(),
              "3",
              "string first startIndex should be interpreted as character index");

    ASSERT_EQ(eval_ok(interp, "string last 中 a中b中", "string last").as_string(),
              "3",
              "string last should return character index");
    ASSERT_EQ(eval_ok(interp, "string last 中 a中b中 2", "string last with bound").as_string(),
              "1",
              "string last lastIndex should be interpreted as character index");

    ASSERT_EQ(eval_ok(interp, "string range a中b文c 1 3", "string range mid").as_string(),
              "中b文",
              "string range should slice by character index");
    ASSERT_EQ(eval_ok(interp, "string range a中b文c -2 1", "string range with negative first").as_string(),
              "a中",
              "negative first index should clamp to 0");
    ASSERT_EQ(eval_ok(interp, "string range a中b文c 2 -1", "string range negative last").as_string(),
              "",
              "negative last index should produce empty result");
END_TEST

TEST(string_option_and_map_semantics)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "string compare -nocase -length 2 Abc aBz", "string compare options").as_string(),
              "0",
              "string compare should honor -nocase and -length");
    ASSERT_EQ(eval_ok(interp, "string equal -nocase -length 2 Abc aBz", "string equal options").as_string(),
              "1",
              "string equal should honor -nocase and -length");

    ASSERT_EQ(eval_ok(interp, "string map -nocase {a X} Aa", "string map nocase").as_string(),
              "XX",
              "string map -nocase should match case-insensitively");

    auto bad_compare = interp.eval("string compare -foo a b");
    ASSERT_TRUE(!bad_compare.has_value(), "unknown compare option should fail");
    ASSERT_TRUE(bad_compare.error().value().as_string().find("bad option \"-foo\": must be -nocase or -length") !=
                    std::string::npos,
                "compare should report canonical bad-option message");

    auto bad_equal = interp.eval("string equal -length a b");
    ASSERT_TRUE(!bad_equal.has_value(), "missing -length value in equal should fail");
    ASSERT_TRUE(bad_equal.error().value().as_string().find("wrong # args") != std::string::npos,
                "equal should report wrong # args for missing -length argument");

    auto bad_map = interp.eval("string map -bad {a X} a");
    ASSERT_TRUE(!bad_map.has_value(), "unknown map option should fail");
    ASSERT_TRUE(bad_map.error().value().as_string().find("bad option \"-bad\": must be -nocase") != std::string::npos,
                "map should report canonical bad-option message");
END_TEST

int main() {
    std::cout << "=== Testing String Semantics ===" << std::endl << std::endl;

    test_string_char_index_semantics();
    test_string_option_and_map_semantics();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

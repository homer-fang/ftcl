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

TEST(dict_create_set_get_unset_nested)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set d [dict create a 1 b 2]; list [dict get $d a] [dict get $d b] [dict size $d]",
                      "dict create/get/size")
                  .as_string(),
              "1 2 2",
              "dict create should build dictionary values usable by get/size");

    ASSERT_EQ(eval_ok(interp, "dict set d c x 3; dict get $d c x", "nested dict set/get").as_string(),
              "3",
              "dict set should support nested path insertion when intermediate key is absent");

    ASSERT_EQ(eval_ok(interp, "dict unset d c x; dict exists $d c x", "nested dict unset").as_string(),
              "0",
              "dict unset should remove nested key");
END_TEST

TEST(dict_path_and_error_semantics)
    auto interp = new_interp_with_stdlib();

    auto bad_create = interp.eval("dict create a");
    ASSERT_TRUE(!bad_create.has_value(), "dict create with odd arguments should fail");
    ASSERT_TRUE(bad_create.error().value().as_string().find("wrong # args") != std::string::npos,
                "dict create odd-arg error should be a wrong # args message");

    ASSERT_EQ(eval_ok(interp, "set d [dict create a 1 a 2]; list [dict get $d a] [dict size $d]",
                      "duplicate keys in dict create")
                  .as_string(),
              "2 1",
              "later duplicate keys in dict create should overwrite earlier entries");

    ASSERT_EQ(eval_ok(interp, "set d [dict create a 1]", "reset dict").as_string(),
              "a 1",
              "setup should succeed");

    auto set_through_scalar = interp.eval("dict set d a q 9");
    ASSERT_TRUE(!set_through_scalar.has_value(), "nested dict set through non-dict leaf should fail");
    ASSERT_TRUE(set_through_scalar.error().value().as_string().find("missing value to go with key") != std::string::npos,
                "nested dict set through non-dict should report dictionary parse failure");

    auto unset_missing_nested = interp.eval("dict unset d z q");
    ASSERT_TRUE(!unset_missing_nested.has_value(), "nested dict unset through missing path should fail");
    ASSERT_TRUE(unset_missing_nested.error().value().as_string().find("not known in dictionary") != std::string::npos,
                "nested dict unset missing path should report unknown key");
END_TEST

int main() {
    std::cout << "=== Testing Dict Semantics ===" << std::endl << std::endl;

    test_dict_create_set_get_unset_nested();
    test_dict_path_and_error_semantics();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

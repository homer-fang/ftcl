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

TEST(array_basic_set_get_size_unset)
    auto interp = new_interp_with_stdlib();

    ASSERT_TRUE(eval_ok(interp, "array set a {k1 v1 k2 v2}", "array set").as_string().empty(),
                "array set should succeed");

    ASSERT_EQ(eval_ok(interp, "list [array exists a] [array size a] [set a(k1)]", "array basic reads").as_string(),
              "1 2 v1",
              "array exists/size and element lookup should be consistent");

    ASSERT_TRUE(eval_ok(interp, "array unset a k1", "array unset element").as_string().empty(),
                "array unset with index should succeed");
    ASSERT_EQ(eval_ok(interp, "array size a", "array size after unset").as_string(),
              "1",
              "array size should decrease after unsetting an element");
END_TEST

TEST(array_set_indexed_name_special_case)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "catch {array set a(x) {k v}} msg; list [set msg] [array exists a] [array size a]",
                      "indexed array set on absent var")
                  .as_string(),
              "{can't set \"a(x)\": variable isn't array} 1 0",
              "array set with indexed varName should create empty array then error");

    auto interp2 = new_interp_with_stdlib();
    ASSERT_EQ(eval_ok(interp2,
                      "set a 1; catch {array set a(x) {k v}} msg; set msg",
                      "indexed array set on scalar var")
                  .as_string(),
              "can't set \"a\": variable isn't array",
              "if base variable is scalar, indexed array set should report scalar/array conflict");
END_TEST

int main() {
    std::cout << "=== Testing Array Semantics ===" << std::endl << std::endl;

    test_array_basic_set_get_size_unset();
    test_array_set_indexed_name_special_case();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

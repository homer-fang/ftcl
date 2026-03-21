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

TEST(bare_and_quoted_variable_substitution)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "set x 42; set y a$x.b; set y", "bare var substitution").as_string(),
              "a42.b",
              "bare word should substitute $x inside token");

    ASSERT_EQ(eval_ok(interp, "set y \"a${x}b\"; set y", "quoted braced var substitution").as_string(),
              "a42b",
              "quoted word should substitute ${x} inside token");

    ASSERT_EQ(eval_ok(interp, "set y a$.b; set y", "non-var dollar").as_string(),
              "a$.b",
              "non-variable $ should remain literal");
END_TEST

TEST(command_substitution_in_words)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "set y a[list b]c; set y", "bare command substitution").as_string(),
              "abc",
              "bare word should support [script] substitution");

    ASSERT_EQ(eval_ok(interp, "set y \"a[list b]c\"; set y", "quoted command substitution").as_string(),
              "abc",
              "quoted word should support [script] substitution");
END_TEST

TEST(braced_words_disable_substitution)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "set x 9; set y {$x [list b]}; set y", "braced literal").as_string(),
              "$x [list b]",
              "braced words should not substitute variables or scripts");
END_TEST

TEST(array_substitution)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "set a(1) foo; set y $a(1); set y", "array direct index").as_string(),
              "foo",
              "$a(1) should resolve array element");

    ASSERT_EQ(eval_ok(interp, "set idx 1; set y $a($idx); set y", "array index substitution").as_string(),
              "foo",
              "$a($idx) should evaluate index word before array lookup");

    ASSERT_EQ(eval_ok(interp, "set y ${a(1)}; set y", "braced array variable").as_string(),
              "foo",
              "${a(1)} should resolve array element");
END_TEST

TEST(expand_operator)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "list x {*}[list a b c] y", "expand from command substitution").as_string(),
              "x a b c y",
              "{*}[list ...] should expand list elements into surrounding command");

    ASSERT_EQ(eval_ok(interp, "list {*}{a b c}", "expand from braced list").as_string(),
              "a b c",
              "{*}{a b c} should expand braced list elements");

    ASSERT_EQ(eval_ok(interp, "set x {*} ; set x", "literal {*}").as_string(),
              "*",
              "standalone {*} followed by whitespace should be literal asterisk");
END_TEST

TEST(parser_error_paths)
    auto interp = new_interp_with_stdlib();

    auto missing_brace = interp.eval("set x {abc");
    ASSERT_TRUE(!missing_brace.has_value(), "missing close brace should fail");
    ASSERT_TRUE(missing_brace.error().value().as_string().find("missing close-brace") != std::string::npos,
                "error should mention missing close-brace");

    auto missing_bracket = interp.eval("set x [list abc");
    ASSERT_TRUE(!missing_bracket.has_value(), "missing close bracket should fail");
    ASSERT_TRUE(missing_bracket.error().value().as_string().find("missing close-bracket") != std::string::npos,
                "error should mention missing close-bracket");
END_TEST

int main() {
    std::cout << "=== Testing Parser/Substitution Semantics ===" << std::endl << std::endl;

    test_bare_and_quoted_variable_substitution();
    test_command_substitution_in_words();
    test_braced_words_disable_substitution();
    test_array_substitution();
    test_expand_operator();
    test_parser_error_paths();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

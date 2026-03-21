#include "eval_ptr.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>

// Simple test framework (kept consistent with existing tests in this repo).
#define TEST(name) \
    void test_##name() { \
        std::cout << "Testing " #name "..." << std::endl;

#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  [OK] " << message << std::endl; \
    }

#define END_TEST \
    std::cout << "  [OK] Test passed!" << std::endl << std::endl; \
    }

using namespace ftcl;

static void assert_opt_eq(const std::optional<char32_t>& v, char32_t expected, const char* message) {
    ASSERT(v.has_value(), message);
    ASSERT(*v == expected, message);
}

TEST(bracket_term)
    EvalPtr ctx("123");
    ASSERT(!ctx.is_bracket_term(), "default bracket_term should be false");
    ctx.set_bracket_term(true);
    ASSERT(ctx.is_bracket_term(), "set_bracket_term(true) should take effect");
END_TEST

TEST(no_eval)
    EvalPtr ctx("123");
    ASSERT(!ctx.is_no_eval(), "default no_eval should be false");
    ctx.set_no_eval(true);
    ASSERT(ctx.is_no_eval(), "set_no_eval(true) should take effect");
    ctx.set_no_eval(false);
    ASSERT(!ctx.is_no_eval(), "set_no_eval(false) should take effect");
END_TEST

TEST(next_is)
    EvalPtr ctx("123");
    ASSERT(ctx.next_is(U'1'), "next_is('1') should be true at beginning");
    ASSERT(!ctx.next_is(U'2'), "next_is('2') should be false at beginning");

    EvalPtr empty("");
    ASSERT(!empty.next_is(U'1'), "next_is on empty input should be false");
END_TEST

TEST(at_end)
    EvalPtr ctx("123");
    ASSERT(!ctx.at_end(), "non-empty input should not be at_end");

    EvalPtr empty("");
    ASSERT(empty.at_end(), "empty input should be at_end");
END_TEST

TEST(at_end_of_script)
    EvalPtr ctx("123");
    ASSERT(!ctx.at_end_of_script(), "plain non-empty input should not be at end of script");

    EvalPtr empty("");
    ASSERT(empty.at_end_of_script(), "empty input should be at end of script");

    EvalPtr bracket("]");
    ASSERT(!bracket.at_end_of_script(), "without bracket_term, leading ']' is just text");
    bracket.set_bracket_term(true);
    ASSERT(bracket.at_end_of_script(), "with bracket_term, leading ']' ends script");
END_TEST

TEST(at_end_of_command)
    EvalPtr ctx("123");
    ASSERT(!ctx.at_end_of_command(), "plain text should not be end of command");

    EvalPtr semi(";123");
    ASSERT(semi.at_end_of_command(), "leading ';' should be end of command");

    EvalPtr newline("\n123");
    ASSERT(newline.at_end_of_command(), "leading newline should be end of command");

    EvalPtr bracket("]123");
    ASSERT(!bracket.at_end_of_command(), "without bracket_term, leading ']' is not end of command");
    bracket.set_bracket_term(true);
    ASSERT(bracket.at_end_of_command(), "with bracket_term, leading ']' is end of command");
END_TEST

TEST(next_is_block_white)
    EvalPtr ctx("123");
    ASSERT(!ctx.next_is_block_white(), "digit should not be block whitespace");

    EvalPtr spaces(" 123");
    ASSERT(spaces.next_is_block_white(), "space should be block whitespace");

    EvalPtr newline("\n123");
    ASSERT(newline.next_is_block_white(), "newline should be block whitespace");
END_TEST

TEST(skip_block_white)
    EvalPtr ctx("123");
    ctx.skip_block_white();
    ASSERT(ctx.next_is(U'1'), "skip_block_white should keep pointer at first non-whitespace");

    EvalPtr spaces("   123");
    spaces.skip_block_white();
    ASSERT(spaces.next_is(U'1'), "skip_block_white should skip leading spaces");

    EvalPtr mixed(" \n 123");
    mixed.skip_block_white();
    ASSERT(mixed.next_is(U'1'), "skip_block_white should skip spaces and newline");
END_TEST

TEST(next_is_line_white)
    EvalPtr ctx("123");
    ASSERT(!ctx.next_is_line_white(), "digit should not be line whitespace");

    EvalPtr spaces(" 123");
    ASSERT(spaces.next_is_line_white(), "space should be line whitespace");

    EvalPtr newline("\n123");
    ASSERT(!newline.next_is_line_white(), "newline should not be line whitespace");
END_TEST

TEST(skip_line_white)
    EvalPtr ctx("123");
    ctx.skip_line_white();
    ASSERT(ctx.next_is(U'1'), "skip_line_white should keep pointer at first non-whitespace");

    EvalPtr spaces("   123");
    spaces.skip_line_white();
    ASSERT(spaces.next_is(U'1'), "skip_line_white should skip leading spaces");

    EvalPtr mixed(" \n 123");
    mixed.skip_line_white();
    ASSERT(mixed.next_is(U'\n'), "skip_line_white should stop before newline");
END_TEST

TEST(next_is_varname_char)
    EvalPtr alpha("a!");
    ASSERT(alpha.next_is_varname_char(), "alphabetic char should be valid varname char");

    EvalPtr under("_x");
    ASSERT(under.next_is_varname_char(), "underscore should be valid varname char");

    EvalPtr digit("9x");
    ASSERT(digit.next_is_varname_char(), "digit should be valid varname char");

    EvalPtr other("-x");
    ASSERT(!other.next_is_varname_char(), "dash should not be valid varname char");

    EvalPtr empty("");
    ASSERT(!empty.next_is_varname_char(), "empty input should be false");
END_TEST

TEST(skip_comment)
    EvalPtr a("123");
    ASSERT(!a.skip_comment(), "non-comment should return false");
    ASSERT(a.next_is(U'1'), "pointer should stay at first char when no comment");

    EvalPtr b(" #123");
    ASSERT(!b.skip_comment(), "comment only recognized when current char is '#'");
    ASSERT(b.next_is(U' '), "leading space should remain");

    EvalPtr c("#123");
    ASSERT(c.skip_comment(), "hash-start input should be treated as comment");
    ASSERT(c.at_end(), "single-line comment without newline should consume to end");

    EvalPtr d("#1 2 3 \na");
    ASSERT(d.skip_comment(), "comment with newline should be skipped");
    ASSERT(d.next_is(U'a'), "pointer should land on first char after newline");

    EvalPtr e("#1 \\na\nb");
    ASSERT(e.skip_comment(), "backslash sequence inside comment should be handled");
    ASSERT(e.next_is(U'b'), "escaped char should be skipped and continue to next line");

    EvalPtr f("#1 2] 3 \na");
    f.set_bracket_term(true);
    ASSERT(f.skip_comment(), "comment skip should ignore bracket_term while in comment");
    ASSERT(f.next_is(U'a'), "comment should still stop at newline");
END_TEST

TEST(tokenizer_bridge_helpers)
    Tokenizer t("abc");
    assert_opt_eq(t.next(), U'a', "next() should consume first char");

    EvalPtr ctx = EvalPtr::from_tokenizer(t);
    ASSERT(ctx.next_is(U'b'), "from_tokenizer should preserve tokenizer position");

    Tokenizer out = ctx.to_tokenizer();
    ASSERT(out.is(U'b'), "to_tokenizer should return tokenizer clone at same position");
END_TEST

int main() {
    std::cout << "=== Testing EvalPtr ===" << std::endl << std::endl;

    test_bracket_term();
    test_no_eval();
    test_next_is();
    test_at_end();
    test_at_end_of_script();
    test_at_end_of_command();
    test_next_is_block_white();
    test_skip_block_white();
    test_next_is_line_white();
    test_skip_line_white();
    test_next_is_varname_char();
    test_skip_comment();
    test_tokenizer_bridge_helpers();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}


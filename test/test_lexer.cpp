#include "lexer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define TEST(name) \
    void test_##name() { \
        std::cout << "Testing " #name "..." << std::endl;

#define ASSERT_TRUE(condition, message) \
    if (!(condition)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  + " << message << std::endl; \
    }

#define ASSERT_EQ(actual, expected, message) \
    if (!((actual) == (expected))) { \
        std::cerr << "FAILED: " << message << " (actual=" << (actual) << ", expected=" << (expected) << ")" \
                  << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  + " << message << std::endl; \
    }

#define END_TEST \
    std::cout << "  + Test passed!" << std::endl << std::endl; \
    }

static std::vector<ftcl::LexTokenType> collect_types(const std::vector<ftcl::LexToken>& tokens) {
    std::vector<ftcl::LexTokenType> out;
    out.reserve(tokens.size());
    for (const auto& tok : tokens) {
        out.push_back(tok.type);
    }
    return out;
}

TEST(default_mode_tokens_and_comments)
    ftcl::Lexer lexer("set x 1\n#c\nputs ok");
    auto tokens = lexer.tokenize_all();
    auto types = collect_types(tokens);

    std::vector<ftcl::LexTokenType> expected = {
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::WhiteSpace,
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::WhiteSpace,
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::Newline,
        ftcl::LexTokenType::Comment,
        ftcl::LexTokenType::Newline,
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::WhiteSpace,
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::EndOfInput,
    };

    ASSERT_EQ(types.size(), expected.size(), "token count should match");
    for (std::size_t i = 0; i < types.size(); ++i) {
        ASSERT_EQ(static_cast<int>(types[i]), static_cast<int>(expected[i]), "token type sequence should match");
    }

    ASSERT_TRUE(tokens[6].text == "#c", "comment token text should match");
    ASSERT_TRUE(tokens[8].text == "puts", "third-line command should be tokenized");
    ASSERT_EQ(tokens[8].span.start_line, static_cast<std::size_t>(3), "puts start line should be 3");
    ASSERT_EQ(tokens[8].span.start_column, static_cast<std::size_t>(1), "puts start column should be 1");
    ASSERT_EQ(tokens[8].span.end_column, static_cast<std::size_t>(5), "puts end column should be 5 (exclusive)");
END_TEST

TEST(hash_inside_word_is_not_comment)
    ftcl::Lexer lexer("set a#b");
    auto tokens = lexer.tokenize_all();
    ASSERT_EQ(tokens.size(), static_cast<std::size_t>(4), "expected 4 tokens including EOF");
    ASSERT_EQ(static_cast<int>(tokens[2].type), static_cast<int>(ftcl::LexTokenType::WordText), "a#b should stay word");
    ASSERT_TRUE(tokens[2].text == "a#b", "hash should stay inside word when not at command start");
END_TEST

TEST(quoted_mode_keeps_spaces_and_semicolons_in_text)
    ftcl::Lexer lexer("puts \"a b;c\"");
    auto tokens = lexer.tokenize_all();
    auto types = collect_types(tokens);

    std::vector<ftcl::LexTokenType> expected = {
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::WhiteSpace,
        ftcl::LexTokenType::Quote,
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::Quote,
        ftcl::LexTokenType::EndOfInput,
    };

    ASSERT_EQ(types.size(), expected.size(), "token count should match");
    for (std::size_t i = 0; i < types.size(); ++i) {
        ASSERT_EQ(static_cast<int>(types[i]), static_cast<int>(expected[i]), "quoted token sequence should match");
    }

    ASSERT_TRUE(tokens[3].text == "a b;c", "quoted body should keep space and semicolon as text");
END_TEST

TEST(quoted_mode_emits_substitution_markers)
    ftcl::Lexer lexer("puts \"$x[expr 1]\"");
    auto tokens = lexer.tokenize_all();
    auto types = collect_types(tokens);

    std::vector<ftcl::LexTokenType> expected = {
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::WhiteSpace,
        ftcl::LexTokenType::Quote,
        ftcl::LexTokenType::Dollar,
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::LBracket,
        ftcl::LexTokenType::WordText,
        ftcl::LexTokenType::RBracket,
        ftcl::LexTokenType::Quote,
        ftcl::LexTokenType::EndOfInput,
    };

    ASSERT_EQ(types.size(), expected.size(), "token count should match");
    for (std::size_t i = 0; i < types.size(); ++i) {
        ASSERT_EQ(static_cast<int>(types[i]), static_cast<int>(expected[i]), "quoted substitution token sequence");
    }

    ASSERT_TRUE(tokens[4].text == "x", "variable name token should be x");
    ASSERT_TRUE(tokens[6].text == "expr 1", "bracket script body should be plain text token");
END_TEST

TEST(span_tracks_utf8_by_codepoint_columns)
    ftcl::Lexer lexer("变量x\nok");
    auto tokens = lexer.tokenize_all();

    ASSERT_TRUE(tokens[0].text == "变量x", "utf8 token text should match");
    ASSERT_EQ(tokens[0].span.start_line, static_cast<std::size_t>(1), "utf8 token start line");
    ASSERT_EQ(tokens[0].span.start_column, static_cast<std::size_t>(1), "utf8 token start col");
    ASSERT_EQ(tokens[0].span.end_column, static_cast<std::size_t>(4), "three code points should end at col 4");
    ASSERT_EQ(tokens[2].span.start_line, static_cast<std::size_t>(2), "token after newline should be line 2");
    ASSERT_EQ(tokens[2].span.start_column, static_cast<std::size_t>(1), "token after newline should start at col 1");
END_TEST

int main() {
    std::cout << "=== Testing Lexer ===" << std::endl << std::endl;

    test_default_mode_tokens_and_comments();
    test_hash_inside_word_is_not_comment();
    test_quoted_mode_keeps_spaces_and_semicolons_in_text();
    test_quoted_mode_emits_substitution_markers();
    test_span_tracks_utf8_by_codepoint_columns();

    std::cout << "=== All Lexer tests passed! ===" << std::endl;
    return 0;
}

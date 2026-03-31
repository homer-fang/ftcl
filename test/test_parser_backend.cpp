#include "parser.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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

static Script parse_or_die(std::string_view script, ParserBackend backend, const std::string& label) {
    auto parsed = parse_script_with_backend(script, backend);
    if (!parsed.has_value()) {
        std::cerr << "FAILED: " << label << std::endl;
        std::cerr << "  parser error: " << parsed.error().value().as_string() << std::endl;
        std::exit(1);
    }
    return *parsed;
}

static void set_parser_backend_env(const char* value) {
#if defined(_WIN32)
    _putenv_s("FTCL_PARSER_BACKEND", value);
#else
    setenv("FTCL_PARSER_BACKEND", value, 1);
#endif
}

static void unset_parser_backend_env() {
#if defined(_WIN32)
    _putenv_s("FTCL_PARSER_BACKEND", "");
#else
    unsetenv("FTCL_PARSER_BACKEND");
#endif
}

TEST(legacy_and_adapter_match_on_valid_scripts)
    const std::vector<std::string> cases = {
        "set x 1",
        "set x 1; set y 2",
        "set x {a b c}",
        "set x {a\nb}",
        "set x {a\\tb}",
        "set x {a\\\nb}",
        "set x {# not-comment-in-brace}",
        "set x \"a b c\"",
        "set x \"a\\tb\"",
        "set x \"a\\\nb\"",
        "set y $x",
        "set y ${x}",
        "set y a$x.b",
        "set y $x.b",
        "set y \"$x.b\"",
        "set y a${x}b",
        "set a(1) foo; set y $a(1)",
        "set a(foo) bar; set y $a(foo).baz",
        "set a(foo) bar; set y \"$a(foo).baz\"",
        "set a(1) foo; set idx 1; set y $a($idx)",
        "set a(1) foo; set i {[list 1]}; set y $a($i)",
        "set y [list a b c]",
        "set y [list [list a] [list b]]",
        "set y [list a\\;b]",
        "set y [list a; b]",
        "set y \"a[list b]c\"",
        "set y [set z 3]",
        "set y \"[$x]\"",
        "set y {[list b]}",
        "set y {$x [list b]}",
        "set y \"#not-comment\"",
        "list {*}[list a b c] y",
        "list {*}{a b c}",
        "list {*}$x",
        "if 1 {set x 1}; # comment\nputs $x",
        "# leading comment\nset x 1",
        "\n\nset x 1\nputs $x",
        "set x 1 ; # after semicolon comment\nset y 2",
        "set x 1\n# whole-line comment\nset y 2",
        "set x \"\\\"quoted\\\"\"",
        "set x [list {a b} {c d}]",
        "set x \\#hash",
        "set x a\\;b",
        "set x a\\ b; puts $x",
        "set x [list a [list b [list c]]]",
        "set x {[set y 1]}",
        "set x \"a[list [list b c]]d\"",
        "set x [list a \\\n b]",
        "set x a\\ b; puts $x",
    };

    ASSERT_TRUE(cases.size() >= 30, "regression set should include at least 30 mixed scripts");

    for (const auto& script : cases) {
        auto legacy = parse_or_die(script, ParserBackend::Legacy, "legacy parse should succeed");
        auto adapter = parse_or_die(script, ParserBackend::TokenStreamAdapter, "adapter parse should succeed");
        const std::string adapter_msg = "legacy and adapter AST should be equal: " + script;

        ASSERT_TRUE(parser_scripts_equal(legacy, adapter), adapter_msg);

        auto shadow = parse_or_die(script, ParserBackend::ShadowCompare, "shadow parse should succeed");
        const std::string shadow_msg = "shadow backend should return legacy-equivalent AST: " + script;
        ASSERT_TRUE(parser_scripts_equal(legacy, shadow), shadow_msg);
    }
END_TEST

TEST(legacy_and_adapter_match_on_error_scripts)
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"set x {abc", "missing close-brace"},
        {"set x [list abc", "missing close-bracket"},
        {"set x \"abc", "missing \""},
        {"set x ${abc", "missing close-brace for variable name"},
        {"set x $a(1", "missing close-paren for array index"},
        {"set x {a}b", "extra characters after close-brace"},
        {"set x \"a\"b", "extra characters after close-quote"},
    };

    for (const auto& [script, keyword] : cases) {
        auto legacy = parse_script_with_backend(script, ParserBackend::Legacy);
        auto adapter = parse_script_with_backend(script, ParserBackend::TokenStreamAdapter);
        auto shadow = parse_script_with_backend(script, ParserBackend::ShadowCompare);

        ASSERT_TRUE(!legacy.has_value(), "legacy should fail on invalid script");
        ASSERT_TRUE(!adapter.has_value(), "adapter should fail on invalid script");
        ASSERT_TRUE(!shadow.has_value(), "shadow should fail on invalid script");
        ASSERT_TRUE(
            legacy.error().value().as_string().find(keyword) != std::string::npos,
            "legacy error should contain expected keyword");
        ASSERT_TRUE(
            adapter.error().value().as_string().find(keyword) != std::string::npos,
            "adapter error should contain expected keyword");
        ASSERT_TRUE(
            shadow.error().value().as_string().find(keyword) != std::string::npos,
            "shadow error should contain expected keyword");
    }
END_TEST

TEST(token_stream_adapter_exposes_tokens)
    const std::string script = "set x 1\nputs $x";
    ParserTokenStreamAdapter adapter(script);
    const auto& tokens = adapter.tokens();
    ASSERT_TRUE(!tokens.empty(), "adapter should capture tokens");
    ASSERT_TRUE(tokens.back().type == LexTokenType::EndOfInput, "last token should be EOF");
END_TEST

TEST(default_backend_runtime_switch)
    const std::string script = "set x 1; set y {$x [list b]}";

    unset_parser_backend_env();
    auto compile_default = parser_compile_time_default_backend();
    auto default_parsed = parse_script(script);
    auto compile_default_parsed = parse_script_with_backend(script, compile_default);
    ASSERT_TRUE(default_parsed.has_value() && compile_default_parsed.has_value(), "default backend should parse");
    ASSERT_TRUE(
        parser_scripts_equal(*default_parsed, *compile_default_parsed),
        "parse_script should follow compile-time default when env is unset");

    set_parser_backend_env("token_stream");
    auto env_token_stream = parse_script(script);
    auto explicit_token_stream = parse_script_with_backend(script, ParserBackend::TokenStreamAdapter);
    ASSERT_TRUE(env_token_stream.has_value() && explicit_token_stream.has_value(), "token_stream env should parse");
    ASSERT_TRUE(
        parser_scripts_equal(*env_token_stream, *explicit_token_stream),
        "token_stream env should match explicit token stream backend");

    set_parser_backend_env("shadow");
    auto env_shadow = parse_script(script);
    ASSERT_TRUE(env_shadow.has_value(), "shadow env should parse valid script");

    set_parser_backend_env("unknown-backend");
    auto env_unknown = parse_script(script);
    auto fallback = parse_script_with_backend(script, compile_default);
    ASSERT_TRUE(env_unknown.has_value() && fallback.has_value(), "unknown env should fall back to compile default");
    ASSERT_TRUE(
        parser_scripts_equal(*env_unknown, *fallback),
        "unknown env backend should fall back to compile default backend");

    unset_parser_backend_env();
END_TEST

int main() {
    std::cout << "=== Testing Parser Backends ===" << std::endl << std::endl;

    test_legacy_and_adapter_match_on_valid_scripts();
    test_legacy_and_adapter_match_on_error_scripts();
    test_token_stream_adapter_exposes_tokens();
    test_default_backend_runtime_switch();

    std::cout << "=== All parser backend tests passed! ===" << std::endl;
    return 0;
}

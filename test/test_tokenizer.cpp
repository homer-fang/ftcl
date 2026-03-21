#include "tokenizer.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <string>
// 根因：在 C++20/23 里，u8"..." 字面量的类型是 const char8_t[N]。
// 但 Tokenizer 的构造函数只接受 std::string_view（也就是 std::basic_string_view<char>）。
// char8_t* 不能隐式转换成 char*/std::string_view，所以编译器报错：no matching function for call to Tokenizer::Tokenizer(const char8_t [...])。
// 我们是如何解决的
// 不改库接口（不动 Tokenizer），只改测试代码：在 test/test_tokenizer.cpp 里新增了一个小工具函数 u8sv(...)，把 char8_t 的 UTF-8 字节序列 按字节 reinterpret 成 std::string_view：
// 把 Tokenizer t(u8"a变量"); 改为 Tokenizer t(u8sv(u8"a变量"));
// 把 Tokenizer t(u8"a🙂b"); 改为 Tokenizer t(u8sv(u8"a🙂b"));
// 然后在 WSL 下重新构建并运行 TokenizerTest，编译与测试均通过。
// Simple test framework (kept consistent with other tests in this repo)
#define TEST(name) \
    void test_##name() { \
        std::cout << "Testing " #name "..." << std::endl;

#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  ✓ " << message << std::endl; \
    }

#define END_TEST \
    std::cout << "  ✓ Test passed!" << std::endl << std::endl; \
    }

static void assert_opt_eq(const std::optional<char32_t>& v, char32_t expected, const char* message) {
    ASSERT(v.has_value(), message);
    ASSERT(*v == expected, message);
}

template <std::size_t N>
static std::string_view u8sv(const char8_t (&s)[N]) {
    // Treat UTF-8 bytes in a char8_t string literal as a char string_view.
    return std::string_view(reinterpret_cast<const char*>(s), N - 1);
}

TEST(peek_next_ascii_and_utf8)
    // "a变量" where '变' and '量' are multi-byte in UTF-8.
    Tokenizer t(u8sv(u8"a变量"));

    assert_opt_eq(t.peek(), U'a', "peek() should see 'a'");
    assert_opt_eq(t.next(), U'a', "next() should return 'a'");

    // After consuming ASCII, index advances by 1 byte; next code point is a CJK character.
    ASSERT(t.peek().has_value(), "peek() should have value after 'a'");
    ASSERT(t.peek().value() != U'a', "peek() should not still be 'a'");

    auto c1 = t.next();
    auto c2 = t.next();
    ASSERT(c1.has_value() && c2.has_value(), "next() should return two code points for '变量'");
    ASSERT(t.at_end(), "at_end() should be true after consuming all code points");
    ASSERT(!t.next().has_value(), "next() at end should return nullopt");
END_TEST

TEST(mark_and_token)
    Tokenizer t("abc");
    std::size_t m0 = t.mark();
    ASSERT(m0 == 0, "mark() at start should be 0");

    t.next(); // 'a'
    t.next(); // 'b'

    ASSERT(t.token(m0) == "ab", "token(mark) should return consumed substring");

    std::size_t m1 = t.mark();
    t.next(); // 'c'
    ASSERT(t.token(m1) == "c", "token(mark) should return substring since mark");
    ASSERT(t.at_end(), "at_end() should be true after consuming 'abc'");
END_TEST

TEST(skip_over_counts_codepoints)
    // "a🙂b": 🙂 is 4 bytes in UTF-8 but should count as 1 code point.
    Tokenizer t(u8sv(u8"a🙂b"));

    t.skip_over(2); // skip 'a' and '🙂'
    assert_opt_eq(t.peek(), U'b', "skip_over(2) should position tokenizer at 'b'");
END_TEST

TEST(skip_while_basic)
    Tokenizer t("   xyz");
    t.skip_while([](char32_t ch) { return ch == U' '; });
    assert_opt_eq(t.peek(), U'x', "skip_while(spaces) should stop at 'x'");
END_TEST

TEST(backslash_subst_common)
    {
        Tokenizer t("\\n");
        ASSERT(t.is(U'\\'), "input should start with backslash");
        ASSERT(t.backslash_subst() == U'\n', "backslash_subst(\\\\n) should return newline");
        ASSERT(t.at_end(), "tokenizer should be at end after consuming escape");
    }
    {
        Tokenizer t("\\t");
        ASSERT(t.backslash_subst() == U'\t', "backslash_subst(\\\\t) should return tab");
    }
    {
        Tokenizer t("\\123"); // octal 123 = decimal 83 = 'S'
        ASSERT(t.backslash_subst() == U'S', "backslash_subst(\\\\123) should parse octal");
    }
    {
        Tokenizer t("\\x41"); // hex 41 = 'A'
        ASSERT(t.backslash_subst() == U'A', "backslash_subst(\\\\x41) should parse hex");
    }
    {
        Tokenizer t("\\u0041"); // 'A'
        ASSERT(t.backslash_subst() == U'A', "backslash_subst(\\\\u0041) should parse unicode escape");
    }
END_TEST

TEST(backslash_subst_edge_cases)
    {
        Tokenizer t("\\");
        ASSERT(t.backslash_subst() == U'\\', "backslash_subst on trailing backslash should return backslash");
        ASSERT(t.at_end(), "tokenizer should be at end after trailing backslash");
    }
    {
        // If there are no hex digits after \x, it should degrade to literal 'x'.
        Tokenizer t("\\xZ");
        ASSERT(t.backslash_subst() == U'x', "backslash_subst(\\\\xZ) should return literal 'x'");
        assert_opt_eq(t.peek(), U'Z', "after fallback, tokenizer should not consume 'Z'");
    }
    {
        // Invalid scalar (surrogate) should fallback to literal 'u' and rewind to before digits.
        // \uD800 is a surrogate.
        Tokenizer t("\\uD800X");
        ASSERT(t.backslash_subst() == U'u', "invalid scalar should fallback to literal 'u'");
        assert_opt_eq(t.peek(), U'D', "after fallback, tokenizer should rewind to first hex digit");
    }
END_TEST

int main() {
    std::cout << "=== Testing Tokenizer ===" << std::endl << std::endl;

    test_peek_next_ascii_and_utf8();
    test_mark_and_token();
    test_skip_over_counts_codepoints();
    test_skip_while_basic();
    test_backslash_subst_common();
    test_backslash_subst_edge_cases();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}



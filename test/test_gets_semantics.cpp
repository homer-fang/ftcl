#include "commands.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#define TEST(name) \
    void test_##name() { \
        std::cout << "Testing " #name "..." << std::endl;

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

class CinRedirectGuard {
public:
    explicit CinRedirectGuard(std::istream& in) : old_(std::cin.rdbuf(in.rdbuf())) {}
    ~CinRedirectGuard() { std::cin.rdbuf(old_); }

private:
    std::streambuf* old_;
};

static Value eval_ok(Interp& interp, const std::string& script, const std::string& label) {
    auto result = interp.eval(script);
    if (!result.has_value()) {
        std::cerr << "FAILED: " << label << std::endl;
        std::cerr << "  error: " << result.error().value().as_string() << std::endl;
        std::exit(1);
    }
    return *result;
}

TEST(gets_reads_line_and_returns_length)
    auto interp = new_interp_with_stdlib();
    std::istringstream input("wasd\n");
    CinRedirectGuard guard(input);

    ASSERT_EQ(eval_ok(interp, "set n [gets stdin line]; list $n $line", "gets line + length").as_string(),
              "4 wasd",
              "gets should read a line and return its length when varName is used");
END_TEST

TEST(gets_eof_returns_minus_one)
    auto interp = new_interp_with_stdlib();
    std::istringstream input("");
    CinRedirectGuard guard(input);

    ASSERT_EQ(eval_ok(interp, "set n [gets stdin line]; list $n [string length $line]", "gets eof").as_string(),
              "-1 0",
              "gets should return -1 and set empty line on EOF");
END_TEST

TEST(gets_requires_stdin_channel)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {gets file1 line} msg]; "
                      "list $code [string first {can not find channel named \"} $msg]",
                      "gets bad channel")
                  .as_string(),
              "1 0",
              "gets should fail for unsupported channels");
END_TEST

int main() {
    std::cout << "=== Testing gets Semantics ===" << std::endl << std::endl;

    test_gets_reads_line_and_returns_length();
    test_gets_eof_returns_minus_one();
    test_gets_requires_stdin_channel();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

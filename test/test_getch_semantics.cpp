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

TEST(getch_reads_single_char)
    auto interp = new_interp_with_stdlib();
    std::istringstream input("wq");
    CinRedirectGuard guard(input);

    ASSERT_EQ(eval_ok(interp, "set a [getch]; set b [getch]; list $a $b", "getch two chars").as_string(),
              "w q",
              "getch should read one character per call");
END_TEST

TEST(getch_varname_returns_length)
    auto interp = new_interp_with_stdlib();
    std::istringstream input("x");
    CinRedirectGuard guard(input);

    ASSERT_EQ(eval_ok(interp, "set n [getch ch]; list $n $ch", "getch varName").as_string(),
              "1 x",
              "getch should set var and return 1 when one character is read");
END_TEST

TEST(getch_eof_and_arity)
    auto interp = new_interp_with_stdlib();
    std::istringstream input("");
    CinRedirectGuard guard(input);

    ASSERT_EQ(eval_ok(interp, "set n [getch ch]; list $n [string length $ch]", "getch eof").as_string(),
              "-1 0",
              "getch should return -1 and empty var value on EOF");

    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {getch a b} msg]; "
                      "list $code [string first {wrong # args: should be \"getch ?-noblock? ?varName?\"} $msg]",
                      "getch arity error")
                  .as_string(),
              "1 0",
              "getch should enforce expected arity");
END_TEST

TEST(getch_non_blocking_when_no_input_ready)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set n [getch -noblock ch]; "
                      "list [expr {$n == 0 || $n == -1}] [string length $ch]",
                      "getch -noblock no input")
                  .as_string(),
              "1 0",
              "getch -noblock should not block and should not produce a character when no key is ready");
END_TEST

int main() {
    std::cout << "=== Testing getch Semantics ===" << std::endl << std::endl;

    test_getch_reads_single_char();
    test_getch_varname_returns_length();
    test_getch_eof_and_arity();
    test_getch_non_blocking_when_no_input_ready();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

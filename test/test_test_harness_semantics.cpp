#include "test_harness.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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
        std::cout << "  [OK] " << message << std::endl; \
    }

#define END_TEST \
    std::cout << "  [OK] Test passed!" << std::endl << std::endl; \
    }

using namespace ftcl;

TEST(test_harness_simple_and_fancy)
    auto interp = new_interp_with_stdlib();

    const auto base = std::filesystem::temp_directory_path() / "ftcl_test_harness_ok";
    const auto sub = base / "sub";
    std::filesystem::create_directories(sub);

    {
        std::ofstream helper(sub / "helper.tcl");
        helper << "set rel 77\n";
    }

    const auto suite = sub / "suite.tcl";
    {
        std::ofstream out(suite);
        out << "test s1 \"simple ok\" {set a 1} -ok 1\n";
        out << "test s2 \"simple err\" {error boom} -error boom\n";
        out << "test f1 \"fancy\" -setup {set x 2} -body {incr x} -cleanup {unset x} -ok 3\n";
        out << "test f2 \"relative source\" -body {source helper.tcl} -ok 77\n";
    }

    std::vector<std::string> args = {suite.string()};
    const bool ok = test_harness(interp, args);
    ASSERT_TRUE(ok, "test_harness should pass for valid simple+fancy tests");

    std::error_code ec;
    std::filesystem::remove(suite, ec);
    std::filesystem::remove(sub / "helper.tcl", ec);
    std::filesystem::remove(sub, ec);
    std::filesystem::remove(base, ec);
END_TEST

TEST(test_harness_invalid_option_reports_error)
    auto interp = new_interp_with_stdlib();

    const auto base = std::filesystem::temp_directory_path() / "ftcl_test_harness_bad";
    std::filesystem::create_directories(base);

    const auto suite = base / "bad.tcl";
    {
        std::ofstream out(suite);
        out << "test b1 \"bad option\" -body {set x 1} -bogus 1\n";
    }

    std::vector<std::string> args = {suite.string()};
    const bool ok = test_harness(interp, args);
    ASSERT_TRUE(!ok, "test_harness should fail when test command has invalid option");

    std::error_code ec;
    std::filesystem::remove(suite, ec);
    std::filesystem::remove(base, ec);
END_TEST

int main() {
    std::cout << "=== Testing Test Harness Semantics ===" << std::endl << std::endl;

    test_test_harness_simple_and_fancy();
    test_test_harness_invalid_option_reports_error();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

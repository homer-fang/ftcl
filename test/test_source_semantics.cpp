#include "commands.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

TEST(source_success_and_missing_file_error)
    auto interp = new_interp_with_stdlib();

    const auto base = std::filesystem::temp_directory_path() / "ftcl_source_semantics";
    std::filesystem::create_directories(base);

    const auto script_file = base / "ok.tcl";
    {
        std::ofstream out(script_file);
        out << "set sourced_value 123\n";
    }

    const std::string src_cmd = "source {" + script_file.string() + "}";
    ASSERT_EQ(eval_ok(interp, src_cmd, "source existing file").as_string(),
              "123",
              "source should execute script and return its result");

    const std::string missing_path = (base / "missing.tcl").string();
    auto missing = interp.eval("source {" + missing_path + "}");
    ASSERT_TRUE(!missing.has_value(), "sourcing missing file should fail");
    ASSERT_TRUE(missing.error().value().as_string().find("couldn't read file \"" + missing_path + "\":") == 0,
                "source error should include filename and system reason");

    std::error_code ec;
    std::filesystem::remove(script_file, ec);
    std::filesystem::remove(base, ec);
END_TEST

int main() {
    std::cout << "=== Testing Source Semantics ===" << std::endl << std::endl;

    test_source_success_and_missing_file_error();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

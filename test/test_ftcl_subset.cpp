#include "test_harness.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace ftcl;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: test_ftcl_tcl_subset <tests dir>" << std::endl;
        return 2;
    }

    const std::filesystem::path tests_dir(argv[1]);
    const std::vector<std::string> files = {
        "append.tcl",    "array.tcl",    "assert_eq.tcl", "break.tcl",   "catch.tcl",   "continue.tcl",
        "dict.tcl",      "error.tcl",    "exit.tcl",      "for.tcl",     "foreach.tcl", "global.tcl",
        "if.tcl",        "incr.tcl",     "info.tcl",      "interp.tcl",  "join.tcl",    "lappend.tcl",
        "lindex.tcl",    "list.tcl",     "llength.tcl",   "parser.tcl",  "rename.tcl",  "return.tcl",
        "set.tcl",       "string.tcl",   "test.tcl",      "throw.tcl",   "unset.tcl",   "while.tcl",
    };

    std::size_t passed = 0;
    std::size_t failed = 0;

    for (const auto& file : files) {
        const auto full = tests_dir / file;
        Interp interp = new_interp_with_stdlib();
        std::vector<std::string> args = {full.string()};

        std::cout << "\n=== Running " << file << " ===" << std::endl;
        if (test_harness(interp, args)) {
            ++passed;
        } else {
            ++failed;
            std::cout << "FAILED FILE: " << file << std::endl;
        }
    }

    std::cout << "\nSubset summary: passed=" << passed << ", failed=" << failed << std::endl;
    return failed == 0 ? 0 : 1;
}

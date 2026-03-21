#include "test_harness.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace ftcl;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: test_ftcl_tcl_suite <all.tcl path>" << std::endl;
        return 2;
    }

    Interp interp = new_interp_with_stdlib();
    std::vector<std::string> args = {argv[1]};

    const bool ok = test_harness(interp, args);
    return ok ? 0 : 1;
}

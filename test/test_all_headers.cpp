#include "lib.hpp"

int main() {
    ftcl::Interp interp;
    auto std_interp = ftcl::new_interp_with_stdlib();
    (void)interp;
    (void)std_interp;
    return 0;
}

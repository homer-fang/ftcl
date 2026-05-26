#include "geometry_test_utils.hpp"

#include <iostream>

using namespace ftcl_geometry_test;

int main() {
    std::cout << "=== UVec CPU/CUDA Consistency Test ===" << std::endl;
    auto interp = ftcl::new_interp_with_stdlib();

#ifdef FTCL_ENABLE_CUDA
    assert_eq(eval_ok(interp,
                      "set v [uvec filled 4 1 cuda:0]; "
                      "set a [list [uvec latest $v] [uvec valid $v cpu] [uvec valid $v cuda:0]]; "
                      "set data [uvec to_list $v]; "
                      "set b [list [uvec latest $v] [uvec valid $v cpu] [uvec valid $v cuda:0]]; "
                      "uvec set $v 1 9; "
                      "set c [list [uvec latest $v] [uvec valid $v cpu] [uvec valid $v cuda:0] [uvec to_list $v]]; "
                      "list $a $data $b $c",
                      "cuda cpu roundtrip")
                  .as_string(),
              "{cuda:0 0 1} {1 1 1 1} {cpu 1 1} {cpu 1 0 {1 9 1 1}}",
              "CPU readback and CPU write should update latest/valid flags predictably");

    assert_eq(eval_ok(interp,
                      "set p [geom uvec_points {{0 0} {3 4}} cuda:0]; "
                      "set d [geom batch_distance $p {0 0} cuda:0]; "
                      "set before [list [uvec latest $d] [uvec valid $d cpu] [uvec valid $d cuda:0]]; "
                      "set values [uvec to_list $d]; "
                      "set after [list [uvec latest $d] [uvec valid $d cpu] [uvec valid $d cuda:0]]; "
                      "list $before $values $after",
                      "cuda geometry output sync")
                  .as_string(),
              "{cuda:0 0 1} {0 5} {cpu 1 1}",
              "CUDA geom outputs should synchronize back to CPU on demand");
#else
    assert_eq(eval_ok(interp,
                      "set code [catch {uvec filled 2 1 cuda:0} msg]; list $code [string first {CUDA backend is not enabled} $msg]",
                      "cuda disabled consistency")
                  .as_string(),
              "1 0",
              "CPU-only builds should report CUDA-disabled errors clearly");
#endif

    std::cout << "=== UVec consistency test passed ===" << std::endl;
    return 0;
}

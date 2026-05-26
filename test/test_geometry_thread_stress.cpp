#include "geometry_test_utils.hpp"

#include <iostream>

using namespace ftcl_geometry_test;

int main() {
    std::cout << "=== Geometry Thread/Stress Integration Test ===" << std::endl;
    auto interp = ftcl::new_interp_with_stdlib();

    assert_eq(eval_ok(interp,
                      "set ch [thread channel create]; "
                      "set script [string cat {set data [geom uvec_points {{0 0} {5 0} {0 5}}]; set q [geom uvec_points {{4 0}}]; set out [geom nearest_point $data $q]; thread channel send } $ch { [uvec to_list $out]; set __result ok}]; "
                      "set tids {}; set i 0; while {$i < 4} {lappend tids [thread spawn $script]; incr i}; "
                      "set results {}; set i 0; while {$i < 4} {lappend results [thread channel recv $ch]; incr i}; "
                      "set waits {}; foreach tid $tids {lappend waits [thread await $tid]}; "
                      "list [llength $results] [lindex $results 0] [lindex $waits 3]",
                      "thread channel geometry workers")
                  .as_string(),
              "4 {1 1} ok",
              "thread workers should execute geom commands and communicate through channels");

    assert_eq(eval_ok(interp,
                      "set i 0; while {$i < 10000} {set p [geom uvec_points {{0 0} {3 4}}]; set d [geom batch_distance $p {0 0}]; set x [uvec get $d 1]; if {$x != 5} {error bad-distance}; uvec destroy $d; uvec destroy $p; incr i}; set i",
                      "cpu create destroy stress")
                  .as_string(),
              "10000",
              "CPU stress loop should create/destroy UVec geometry results without failure");

#ifdef FTCL_ENABLE_CUDA
    assert_eq(eval_ok(interp,
                      "set i 0; while {$i < 500} {set p [geom uvec_points {{0 0} {3 4}} cuda:0]; set d [geom batch_distance $p {0 0} cuda:0]; set x [uvec get $d 1]; if {$x != 5} {error bad-distance}; uvec destroy $d; uvec destroy $p; incr i}; set i",
                      "cuda create destroy stress")
                  .as_string(),
              "500",
              "CUDA stress loop should repeatedly synchronize and destroy geometry UVecs");
#endif

    std::cout << "=== Geometry thread/stress test passed ===" << std::endl;
    return 0;
}

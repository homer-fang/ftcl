#include "geometry_test_utils.hpp"

#include <iostream>

using namespace ftcl_geometry_test;

#ifdef FTCL_ENABLE_CUDA
static void compare(ftcl::Interp& interp, const std::string& cpu, const std::string& cuda, const std::string& label) {
    assert_close_vec(eval_numbers(interp, cuda, label + " cuda"), eval_numbers(interp, cpu, label + " cpu"), 1e-6, label);
}
#endif

int main() {
    std::cout << "=== Geometry Random Differential Test ===" << std::endl;
#ifndef FTCL_ENABLE_CUDA
    std::cout << "  [SKIP] CUDA backend is disabled; random CPU/GPU differential checks require CUDA." << std::endl;
    return 0;
#else
    auto interp = ftcl::new_interp_with_stdlib();
    std::mt19937 rng(20260526);
    std::size_t checks = 0;

    for (int trial = 0; trial < 20; ++trial) {
        const auto a = points_literal(random_points(rng, 8));
        const auto b = points_literal(random_points(rng, 7));
        compare(interp,
                "set a [geom uvec_points " + a + "]; set b [geom uvec_points " + b + "]; set o [geom batch_distance_matrix $a $b]; uvec to_list $o",
                "set a [geom uvec_points " + a + " cuda:0]; set b [geom uvec_points " + b + " cuda:0]; set o [geom batch_distance_matrix $a $b cuda:0]; uvec to_list $o",
                "random batch_distance_matrix trial " + std::to_string(trial));
        ++checks;

        const auto points = points_literal(random_points(rng, 16, -3.0, 3.0));
        compare(interp,
                "set p [geom uvec_points " + points + "]; set o [geom batch_point_in_polygon $p {{-1 -1} {1 -1} {1 1} {-1 1}}]; uvec to_list $o",
                "set p [geom uvec_points " + points + " cuda:0]; set o [geom batch_point_in_polygon $p {{-1 -1} {1 -1} {1 1} {-1 1}} cuda:0]; uvec to_list $o",
                "random batch_point_in_polygon trial " + std::to_string(trial));
        ++checks;

        const auto s1 = segments_literal(random_segments(rng, 12));
        const auto s2 = segments_literal(random_segments(rng, 12));
        compare(interp,
                "set a [geom uvec_segments " + s1 + "]; set b [geom uvec_segments " + s2 + "]; set o [geom batch_segment_intersect $a $b]; uvec to_list $o",
                "set a [geom uvec_segments " + s1 + " cuda:0]; set b [geom uvec_segments " + s2 + " cuda:0]; set o [geom batch_segment_intersect $a $b cuda:0]; uvec to_list $o",
                "random batch_segment_intersect trial " + std::to_string(trial));
        ++checks;

        const auto p = points_literal(random_points(rng, 12));
        const auto s = segments_literal(random_segments(rng, 12));
        compare(interp,
                "set p [geom uvec_points " + p + "]; set s [geom uvec_segments " + s + "]; set o [geom batch_point_segment_distance $p $s]; uvec to_list $o",
                "set p [geom uvec_points " + p + " cuda:0]; set s [geom uvec_segments " + s + " cuda:0]; set o [geom batch_point_segment_distance $p $s cuda:0]; uvec to_list $o",
                "random batch_point_segment_distance trial " + std::to_string(trial));
        ++checks;

        const auto dataset = points_literal(random_points(rng, 18));
        const auto queries = points_literal(random_points(rng, 9));
        compare(interp,
                "set d [geom uvec_points " + dataset + "]; set q [geom uvec_points " + queries + "]; set o [geom nearest_point $d $q]; uvec to_list $o",
                "set d [geom uvec_points " + dataset + " cuda:0]; set q [geom uvec_points " + queries + " cuda:0]; set o [geom nearest_point $d $q cuda:0]; uvec to_list $o",
                "random nearest_point trial " + std::to_string(trial));
        ++checks;
    }

    std::cout << "  [OK] random differential checks=" << checks << std::endl;
    std::cout << "=== Random differential test passed ===" << std::endl;
    return 0;
#endif
}

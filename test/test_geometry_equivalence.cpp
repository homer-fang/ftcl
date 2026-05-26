#include "geometry_test_utils.hpp"

#include <iostream>

using namespace ftcl_geometry_test;

#ifdef FTCL_ENABLE_CUDA
static void compare_uvec_script(ftcl::Interp& interp,
                                const std::string& cpu_script,
                                const std::string& cuda_script,
                                const std::string& label,
                                double eps = 1e-6) {
    const auto cpu = eval_numbers(interp, cpu_script, label + " cpu");
    const auto cuda = eval_numbers(interp, cuda_script, label + " cuda");
    assert_close_vec(cuda, cpu, eps, label);
}
#endif

int main() {
    std::cout << "=== Geometry CPU/GPU Equivalence Test ===" << std::endl;
#ifndef FTCL_ENABLE_CUDA
    std::cout << "  [SKIP] CUDA backend is disabled; CPU build only validates compilation." << std::endl;
    return 0;
#else
    auto interp = ftcl::new_interp_with_stdlib();

    compare_uvec_script(interp,
        "set a [geom uvec_points {{0 0} {3 4} {-2 5}}]; set b [geom uvec_points {{0 0} {0 4}}]; set o [geom batch_distance_matrix $a $b]; uvec to_list $o",
        "set a [geom uvec_points {{0 0} {3 4} {-2 5}} cuda:0]; set b [geom uvec_points {{0 0} {0 4}} cuda:0]; set o [geom batch_distance_matrix $a $b cuda:0]; uvec to_list $o",
        "batch_distance_matrix CPU/GPU equivalence");

    compare_uvec_script(interp,
        "set p [geom uvec_points {{0.5 0.5} {1 0.5} {2 2} {-1 0}}]; set o [geom batch_point_in_polygon $p {{0 0} {1 0} {1 1} {0 1}}]; uvec to_list $o",
        "set p [geom uvec_points {{0.5 0.5} {1 0.5} {2 2} {-1 0}} cuda:0]; set o [geom batch_point_in_polygon $p {{0 0} {1 0} {1 1} {0 1}} cuda:0]; uvec to_list $o",
        "batch_point_in_polygon CPU/GPU equivalence");

    compare_uvec_script(interp,
        "set a [geom uvec_segments {{{0 0} {2 2}} {{0 0} {1 0}} {{0 0} {0 0}}}]; set b [geom uvec_segments {{{0 2} {2 0}} {{0 1} {1 1}} {{0 0} {1 1}}}]; set o [geom batch_segment_intersect $a $b]; uvec to_list $o",
        "set a [geom uvec_segments {{{0 0} {2 2}} {{0 0} {1 0}} {{0 0} {0 0}}} cuda:0]; set b [geom uvec_segments {{{0 2} {2 0}} {{0 1} {1 1}} {{0 0} {1 1}}} cuda:0]; set o [geom batch_segment_intersect $a $b cuda:0]; uvec to_list $o",
        "batch_segment_intersect CPU/GPU equivalence");

    compare_uvec_script(interp,
        "set p [geom uvec_points {{1 1} {3 0} {0 0}}]; set s [geom uvec_segments {{{0 0} {2 0}} {{0 0} {2 0}} {{1 1} {1 1}}}]; set o [geom batch_point_segment_distance $p $s]; uvec to_list $o",
        "set p [geom uvec_points {{1 1} {3 0} {0 0}} cuda:0]; set s [geom uvec_segments {{{0 0} {2 0}} {{0 0} {2 0}} {{1 1} {1 1}}} cuda:0]; set o [geom batch_point_segment_distance $p $s cuda:0]; uvec to_list $o",
        "batch_point_segment_distance CPU/GPU equivalence");

    compare_uvec_script(interp,
        "set data [geom uvec_points {{0 0} {5 0} {0 5} {-3 -3}}]; set q [geom uvec_points {{4 0} {0 4} {-2 -2}}]; set o [geom nearest_point $data $q]; uvec to_list $o",
        "set data [geom uvec_points {{0 0} {5 0} {0 5} {-3 -3}} cuda:0]; set q [geom uvec_points {{4 0} {0 4} {-2 -2}} cuda:0]; set o [geom nearest_point $data $q cuda:0]; uvec to_list $o",
        "nearest_point CPU/GPU equivalence");

    compare_uvec_script(interp,
        "set p [geom uvec_points {{0 0} {1 0} {5 0} {-1 0}}]; set c [geom uvec_points {{0 0} {5 0}}]; set o [geom range_count_circle $p $c 1.1]; uvec to_list $o",
        "set p [geom uvec_points {{0 0} {1 0} {5 0} {-1 0}} cuda:0]; set c [geom uvec_points {{0 0} {5 0}} cuda:0]; set o [geom range_count_circle $p $c 1.1 cuda:0]; uvec to_list $o",
        "range_count_circle CPU/GPU equivalence");

    compare_uvec_script(interp,
        "set p [geom uvec_points {{0 0} {1 1} {3 3} {-2 -2}}]; set r [geom uvec_aabbs {{{-0.5 -0.5} {1.5 1.5}} {{2 2} {4 4}}}]; set o [geom range_count_rect $p $r]; uvec to_list $o",
        "set p [geom uvec_points {{0 0} {1 1} {3 3} {-2 -2}} cuda:0]; set r [geom uvec_aabbs {{{-0.5 -0.5} {1.5 1.5}} {{2 2} {4 4}}} cuda:0]; set o [geom range_count_rect $p $r cuda:0]; uvec to_list $o",
        "range_count_rect CPU/GPU equivalence");

    compare_uvec_script(interp,
        "set p [geom uvec_points {{0 0} {3 4} {-1 2}}]; set o [geom transform_points $p affine 2 0 1 0 3 -1]; uvec to_list $o",
        "set p [geom uvec_points {{0 0} {3 4} {-1 2}} cuda:0]; set o [geom transform_points $p affine 2 0 1 0 3 -1 cuda:0]; uvec to_list $o",
        "transform_points CPU/GPU equivalence");

    assert_eq(eval_ok(interp, "set p [geom uvec_points {{2 3} {-1 4} {0 -5}}]; geom bbox_reduce $p", "bbox cpu").as_string(),
              eval_ok(interp, "set p [geom uvec_points {{2 3} {-1 4} {0 -5}} cuda:0]; geom bbox_reduce $p cuda:0", "bbox cuda").as_string(),
              "bbox_reduce CPU/GPU equivalence");

    assert_eq(eval_ok(interp, "set p [geom uvec_points {{0 0} {2 0} {0 2} {2 2}}]; geom centroid $p", "centroid cpu").as_string(),
              eval_ok(interp, "set p [geom uvec_points {{0 0} {2 0} {0 2} {2 2}} cuda:0]; geom centroid $p cuda:0", "centroid cuda").as_string(),
              "centroid CPU/GPU equivalence");

    std::cout << "=== All equivalence checks passed ===" << std::endl;
    return 0;
#endif
}

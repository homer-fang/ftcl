#include "geometry_test_utils.hpp"

#include <iostream>

using namespace ftcl_geometry_test;

int main() {
    std::cout << "=== Geometry Edge Case Test ===" << std::endl;
    auto interp = ftcl::new_interp_with_stdlib();

    assert_eq(eval_ok(interp, "set p [geom uvec_points {}]; list [uvec len $p] [uvec to_list $p]", "empty points").as_string(),
              "0 {}",
              "empty point UVec should be representable");

    assert_eq(eval_ok(interp, "set p [geom uvec_points {}]; set code [catch {geom bbox_reduce $p} msg]; list $code $msg", "empty bbox_reduce").as_string(),
              "1 {bbox_reduce requires at least one point}",
              "bbox_reduce should reject empty point sets");

    assert_eq(eval_ok(interp, "set p [geom uvec_points {{1 1} {1 1} {2 2}}]; set q [geom uvec_points {{1 1}}]; set o [geom nearest_point $p $q]; uvec to_list $o", "duplicate nearest").as_string(),
              "0 0",
              "nearest_point should deterministically choose the first duplicate");

    assert_eq(eval_ok(interp, "set t [geom uvec_points {{0 0} {1 1} {2 2}}]; set o [geom batch_orientation $t]; uvec to_list $o", "collinear orientation").as_string(),
              "0",
              "batch_orientation should classify collinear triples");

    assert_eq(eval_ok(interp, "set p [geom uvec_points {{1 0} {0 0} {0.5 0.5}}]; set o [geom batch_point_in_polygon $p {{0 0} {1 0} {1 1} {0 1}}]; uvec to_list $o", "polygon boundary").as_string(),
              "1 1 2",
              "batch_point_in_polygon should identify boundary points");

    auto degenerate = eval_numbers(interp,
                                   "set p [geom uvec_points {{1 1}}]; set s [geom uvec_segments {{{0 0} {0 0}}}]; set o [geom batch_point_segment_distance $p $s]; uvec to_list $o",
                                   "degenerate segment distance");
    assert_close(degenerate[0], std::sqrt(2.0), 1e-9, "degenerate segment should behave as a point");

    assert_eq(eval_ok(interp, "set a [geom uvec_aabbs {{{0 0} {1 1}}}]; set b [geom uvec_aabbs {{{1 0} {2 1}}}]; set o [geom collision_aabb $a $b]; uvec to_list $o", "touching aabb").as_string(),
              "1",
              "AABBs touching at an edge should collide");

    auto large = eval_numbers(interp,
                              "set a [geom uvec_points {{1000000000 -1000000000}}]; set b [geom uvec_points {{-1000000000 -1000000000}}]; set o [geom batch_distance_matrix $a $b]; uvec to_list $o",
                              "large coordinates");
    assert_close(large[0], 2000000000.0, 1e-3, "large coordinate distance should remain numerically stable");

#ifdef FTCL_ENABLE_CUDA
    assert_eq(eval_ok(interp, "set a [geom uvec_aabbs {{{0 0} {1 1}}} cuda:0]; set b [geom uvec_aabbs {{{1 0} {2 1}}} cuda:0]; set o [geom collision_aabb $a $b cuda:0]; uvec to_list $o", "touching aabb cuda").as_string(),
              "1",
              "CUDA AABB edge-touching collision should match CPU semantics");
#endif

    std::cout << "=== Edge case test passed ===" << std::endl;
    return 0;
}

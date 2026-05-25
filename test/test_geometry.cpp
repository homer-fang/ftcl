#include "commands.hpp"
#include "geometry.hpp"

#include <cmath>
#include <cstdlib>
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

#define ASSERT_EQ(actual, expected, message) \
    if ((actual) != (expected)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::cerr << "  expected: [" << expected << "]" << std::endl; \
        std::cerr << "  actual:   [" << actual << "]" << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  [OK] " << message << std::endl; \
    }

#define ASSERT_NEAR(actual, expected, eps, message) \
    if (std::fabs((actual) - (expected)) > (eps)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::cerr << "  expected near: [" << expected << "]" << std::endl; \
        std::cerr << "  actual:        [" << actual << "]" << std::endl; \
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

TEST(cpu_geometry_primitives)
    using ftcl::geom::Point;

    ASSERT_NEAR(ftcl::geom::distance(Point{0, 0}, Point{3, 4}), 5.0, 1e-9, "distance should use Euclidean norm");
    ASSERT_NEAR(ftcl::geom::distance2(Point{0, 0}, Point{3, 4}), 25.0, 1e-9, "distance2 should avoid sqrt");
    ASSERT_EQ(ftcl::geom::orientation(Point{0, 0}, Point{1, 0}, Point{0, 1}), 1, "left turn orientation");
    ASSERT_EQ(ftcl::geom::orientation(Point{0, 0}, Point{0, 1}, Point{1, 0}), -1, "right turn orientation");
    ASSERT_EQ(ftcl::geom::orientation(Point{0, 0}, Point{1, 1}, Point{2, 2}), 0, "collinear orientation");
    ASSERT_TRUE(ftcl::geom::on_segment(Point{0, 0}, Point{2, 2}, Point{1, 1}), "point on segment");
    ASSERT_TRUE(ftcl::geom::segments_intersect(Point{0, 0}, Point{2, 2}, Point{0, 2}, Point{2, 0}), "crossing segments");
    ASSERT_TRUE(!ftcl::geom::segments_intersect(Point{0, 0}, Point{1, 0}, Point{0, 1}, Point{1, 1}), "parallel separated segments");

    auto inter = ftcl::geom::line_intersection(Point{0, 0}, Point{1, 1}, Point{0, 1}, Point{1, 0});
    ASSERT_TRUE(inter.has_value(), "non-parallel lines should intersect");
    ASSERT_NEAR(inter->x, 0.5, 1e-9, "line intersection x");
    ASSERT_NEAR(inter->y, 0.5, 1e-9, "line intersection y");
END_TEST

TEST(cpu_polygon_and_hull_algorithms)
    using ftcl::geom::Point;
    std::vector<Point> square{{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    ASSERT_NEAR(ftcl::geom::polygon_area(square), 1.0, 1e-9, "square area");
    ASSERT_NEAR(ftcl::geom::polygon_signed_area(square), 1.0, 1e-9, "counter-clockwise signed area");
    ASSERT_NEAR(ftcl::geom::polygon_perimeter(square), 4.0, 1e-9, "square perimeter");
    ASSERT_EQ(ftcl::geom::point_location_to_string(ftcl::geom::point_in_polygon(Point{0.5, 0.5}, square)),
              std::string("inside"),
              "point inside polygon");
    ASSERT_EQ(ftcl::geom::point_location_to_string(ftcl::geom::point_in_polygon(Point{1, 0.5}, square)),
              std::string("boundary"),
              "point on polygon boundary");
    ASSERT_EQ(ftcl::geom::point_location_to_string(ftcl::geom::point_in_polygon(Point{2, 2}, square)),
              std::string("outside"),
              "point outside polygon");

    std::vector<Point> cloud{{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0.5, 0.5}, {1, 1}};
    auto hull = ftcl::geom::convex_hull(cloud);
    ASSERT_EQ(hull.size(), static_cast<std::size_t>(4), "convex hull should remove duplicates and inner points");
    ASSERT_TRUE(ftcl::geom::same_point(hull[0], Point{0, 0}), "hull point 0");
    ASSERT_TRUE(ftcl::geom::same_point(hull[1], Point{1, 0}), "hull point 1");
    ASSERT_TRUE(ftcl::geom::same_point(hull[2], Point{1, 1}), "hull point 2");
    ASSERT_TRUE(ftcl::geom::same_point(hull[3], Point{0, 1}), "hull point 3");

    auto box = ftcl::geom::bounding_box(cloud);
    ASSERT_TRUE(box.has_value(), "bounding box should exist for non-empty point set");
    ASSERT_TRUE(ftcl::geom::same_point(box->min, Point{0, 0}), "bbox min");
    ASSERT_TRUE(ftcl::geom::same_point(box->max, Point{1, 1}), "bbox max");

    auto closest = ftcl::geom::closest_pair_distance({Point{0, 0}, Point{2, 0}, Point{5, 0}});
    ASSERT_TRUE(closest.has_value(), "closest pair should exist for at least two points");
    ASSERT_NEAR(*closest, 2.0, 1e-9, "closest pair distance");
END_TEST

TEST(geom_command_scalar_algorithms)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "info cmdtype geom", "geom command type").as_string(),
              "native",
              "geom should be installed as a native command");
    ASSERT_EQ(eval_ok(interp, "geom distance {0 0} {3 4}", "geom distance").as_string(),
              "5",
              "geom distance should return Euclidean distance");
    ASSERT_EQ(eval_ok(interp, "geom distance2 {0 0} {3 4}", "geom distance2").as_string(),
              "25",
              "geom distance2 should return squared distance");
    ASSERT_EQ(eval_ok(interp, "geom orient {0 0} {1 0} {0 1}", "geom orient").as_string(),
              "1",
              "geom orient should expose orientation sign");
    ASSERT_EQ(eval_ok(interp, "geom segment_intersect {0 0} {2 2} {0 2} {2 0}", "geom segment intersect").as_string(),
              "1",
              "geom segment_intersect should detect crossings");
    ASSERT_EQ(eval_ok(interp, "geom on_segment {0 0} {2 2} {1 1}", "geom on_segment").as_string(),
              "1",
              "geom on_segment should detect points on segment");
    ASSERT_EQ(eval_ok(interp, "geom line_intersection {0 0} {1 1} {0 1} {1 0}", "geom line intersection").as_string(),
              "0.5 0.5",
              "geom line_intersection should return intersection point");
END_TEST

TEST(geom_command_polygon_algorithms)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "geom polygon_area {{0 0} {1 0} {1 1} {0 1}}", "geom polygon area").as_string(),
              "1",
              "geom polygon_area should return absolute area");
    ASSERT_EQ(eval_ok(interp, "geom polygon_perimeter {{0 0} {1 0} {1 1} {0 1}}", "geom polygon perimeter").as_string(),
              "4",
              "geom polygon_perimeter should return closed perimeter");
    ASSERT_EQ(eval_ok(interp, "geom point_in_polygon {0.5 0.5} {{0 0} {1 0} {1 1} {0 1}}", "point inside").as_string(),
              "inside",
              "geom point_in_polygon should classify inside point");
    ASSERT_EQ(eval_ok(interp, "geom point_in_polygon {1 0.5} {{0 0} {1 0} {1 1} {0 1}}", "point boundary").as_string(),
              "boundary",
              "geom point_in_polygon should classify boundary point");
    ASSERT_EQ(eval_ok(interp, "geom convex_hull {{0 0} {1 0} {1 1} {0 1} {0.5 0.5}}", "geom convex hull").as_string(),
              "{0 0} {1 0} {1 1} {0 1}",
              "geom convex_hull should return canonical hull order");
    ASSERT_EQ(eval_ok(interp, "geom bbox {{2 3} {-1 4} {0 -5}}", "geom bbox").as_string(),
              "{-1 -5} {2 4}",
              "geom bbox should return min/max points");
    ASSERT_EQ(eval_ok(interp, "geom closest_pair_distance {{0 0} {2 0} {5 0}}", "closest pair").as_string(),
              "2",
              "geom closest_pair_distance should return nearest distance");
END_TEST

TEST(geom_command_error_paths)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {geom distance {0} {1 2}} msg]; list $code $msg",
                      "bad point error")
                  .as_string(),
              "1 {expected point as {x y}}",
              "geom should validate point arity");
    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {geom bbox {}} msg]; list $code $msg",
                      "empty bbox error")
                  .as_string(),
              "1 {cannot compute bounding box of empty point set}",
              "geom bbox should reject empty point sets");
    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {geom closest_pair_distance {{0 0}}} msg]; list $code $msg",
                      "closest pair too small")
                  .as_string(),
              "1 {closest_pair_distance requires at least two points}",
              "closest pair should require two points");
END_TEST

TEST(geom_uvec_points_and_batch_distance)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set pts [geom uvec_points {{0 0} {3 4} {6 8}}]; "
                      "set dist [geom batch_distance $pts {0 0}]; "
                      "list [uvec to_list $pts] [uvec to_list $dist]",
                      "geom uvec batch distance")
                  .as_string(),
              "{0 0 3 4 6 8} {0 5 10}",
              "geom batch_distance should return a UVec handle of distances");

#ifdef FTCL_ENABLE_CUDA
    ASSERT_EQ(eval_ok(interp,
                      "set pts [geom uvec_points {{0 0} {3 4}} cuda:0]; "
                      "set dist [geom batch_distance $pts {0 0} cuda:0]; "
                      "list [list [uvec latest $pts] [uvec valid $pts cuda:0]] [uvec latest $dist] [uvec to_list $dist]",
                      "geom cuda uvec batch distance")
                  .as_string(),
              "{cuda:0 1} cuda:0 {0 5}",
              "geom batch_distance should place output UVec on requested CUDA device");
#else
    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {geom uvec_points {{0 0}} cuda:0} msg]; "
                      "list $code [string first {CUDA backend is not enabled} $msg]",
                      "geom cuda disabled error")
                  .as_string(),
              "1 0",
              "geom uvec_points should report CUDA-disabled errors clearly");
#endif
END_TEST

int main() {
    std::cout << "=== Testing Geometry Semantics ===" << std::endl << std::endl;

    test_cpu_geometry_primitives();
    test_cpu_polygon_and_hull_algorithms();
    test_geom_command_scalar_algorithms();
    test_geom_command_polygon_algorithms();
    test_geom_command_error_paths();
    test_geom_uvec_points_and_batch_distance();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}

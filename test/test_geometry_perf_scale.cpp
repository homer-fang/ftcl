#include "geometry_test_utils.hpp"

#include <iostream>

using namespace ftcl_geometry_test;

static std::vector<std::pair<double, double>> grid_points(std::size_t n) {
    std::vector<std::pair<double, double>> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.emplace_back(static_cast<double>(i % 257) * 0.25, static_cast<double>((i * 37) % 263) * 0.25);
    }
    return out;
}

static double run_len_timed(ftcl::Interp& interp, const std::string& script, const std::string& label, std::size_t expected_len) {
    double elapsed = 0.0;
    ftcl::Value result;
    elapsed = timed_us([&]() { result = eval_ok(interp, script, label); });
    auto parsed = result.as_int_opt();
    if (!parsed.has_value() || static_cast<std::size_t>(*parsed) != expected_len) {
        fail(label + " produced unexpected length");
    }
    return elapsed;
}

int main() {
    std::cout << "=== Geometry Performance Scale Smoke Test ===" << std::endl;
    auto interp = ftcl::new_interp_with_stdlib();

    std::cout << "metric,device,n,q,time_us,throughput_items_per_s" << std::endl;
    for (std::size_t n : {128UL, 512UL, 1024UL}) {
        const auto points = points_literal(grid_points(n));
        const auto queries = points_literal(grid_points(std::max<std::size_t>(16, n / 16)));
        const std::size_t q = std::max<std::size_t>(16, n / 16);

        const double cpu_nearest = run_len_timed(
            interp,
            "set data [geom uvec_points " + points + "]; set q [geom uvec_points " + queries + "]; set out [geom nearest_point $data $q]; uvec len $out",
            "nearest cpu scale " + std::to_string(n),
            q * 2);
        std::cout << "nearest_point,cpu," << n << ',' << q << ',' << cpu_nearest << ',' << (static_cast<double>(n * q) * 1e6 / cpu_nearest) << std::endl;

        const double cpu_range = run_len_timed(
            interp,
            "set data [geom uvec_points " + points + "]; set q [geom uvec_points " + queries + "]; set out [geom range_count_circle $data $q 8.0]; uvec len $out",
            "range cpu scale " + std::to_string(n),
            q);
        std::cout << "range_count_circle,cpu," << n << ',' << q << ',' << cpu_range << ',' << (static_cast<double>(n * q) * 1e6 / cpu_range) << std::endl;

#ifdef FTCL_ENABLE_CUDA
        const double cuda_nearest = run_len_timed(
            interp,
            "set data [geom uvec_points " + points + " cuda:0]; set q [geom uvec_points " + queries + " cuda:0]; set out [geom nearest_point $data $q cuda:0]; uvec len $out",
            "nearest cuda scale " + std::to_string(n),
            q * 2);
        std::cout << "nearest_point,cuda:0," << n << ',' << q << ',' << cuda_nearest << ',' << (static_cast<double>(n * q) * 1e6 / cuda_nearest) << std::endl;

        const double cuda_range = run_len_timed(
            interp,
            "set data [geom uvec_points " + points + " cuda:0]; set q [geom uvec_points " + queries + " cuda:0]; set out [geom range_count_circle $data $q 8.0 cuda:0]; uvec len $out",
            "range cuda scale " + std::to_string(n),
            q);
        std::cout << "range_count_circle,cuda:0," << n << ',' << q << ',' << cuda_range << ',' << (static_cast<double>(n * q) * 1e6 / cuda_range) << std::endl;
#endif
    }

    std::cout << "=== Geometry performance scale smoke test passed ===" << std::endl;
    return 0;
}

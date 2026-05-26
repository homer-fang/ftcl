#include "geometry_test_utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace ftcl_geometry_test;

struct ScaleCase {
    std::size_t n;
    std::size_t q;
    std::size_t rounds;
};

static std::vector<std::pair<double, double>> grid_points(std::size_t n) {
    std::vector<std::pair<double, double>> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.emplace_back(static_cast<double>(i % 1009) * 0.25, static_cast<double>((i * 37) % 1013) * 0.25);
    }
    return out;
}

static std::string perf_mode() {
    const char* mode = std::getenv("FTCL_GEOMETRY_PERF_MODE");
    if (mode == nullptr) {
        return "smoke";
    }
    return std::string(mode);
}

static bool paper_mode() {
    return perf_mode() == "paper";
}

static bool break_even_mode() {
    return perf_mode() == "break_even";
}

static std::vector<ScaleCase> scale_cases() {
    if (paper_mode()) {
        return {
            {2048, 256, 5},
            {8192, 1024, 5},
            {32768, 2048, 3},
        };
    }

    if (break_even_mode()) {
        return {
            {128, 64, 3},
            {192, 64, 3},
            {256, 96, 3},
            {384, 128, 3},
            {512, 160, 3},
            {768, 192, 3},
            {1024, 256, 3},
            {1536, 256, 3},
            {2048, 256, 3},
            {3072, 384, 3},
            {4096, 512, 3},
            {6144, 768, 3},
            {8192, 1024, 3},
        };
    }

    return {
        {128, 16, 1},
        {512, 32, 1},
        {1024, 64, 1},
    };
}

static double median(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    if (values.size() % 2 == 1) {
        return values[mid];
    }
    return (values[mid - 1] + values[mid]) / 2.0;
}

static std::string eval_string(ftcl::Interp& interp, const std::string& script, const std::string& label) {
    return eval_ok(interp, script, label).as_string();
}

static void expect_len(ftcl::Interp& interp, const std::string& script, const std::string& label, std::size_t expected_len) {
    const auto value = eval_ok(interp, script, label);
    auto parsed = value.as_int_opt();
    if (!parsed.has_value() || static_cast<std::size_t>(*parsed) != expected_len) {
        fail(label + " produced unexpected length");
    }
}

static double run_len_median(ftcl::Interp& interp,
                             const std::string& script,
                             const std::string& label,
                             std::size_t expected_len,
                             std::size_t rounds) {
    // One untimed warmup removes first-call cache and CUDA launch setup noise from the measured samples.
    expect_len(interp, script, label + " warmup", expected_len);

    std::vector<double> samples;
    samples.reserve(rounds);
    for (std::size_t i = 0; i < rounds; ++i) {
        double elapsed = 0.0;
        ftcl::Value result;
        elapsed = timed_us([&]() { result = eval_ok(interp, script, label + " sample " + std::to_string(i)); });
        auto parsed = result.as_int_opt();
        if (!parsed.has_value() || static_cast<std::size_t>(*parsed) != expected_len) {
            fail(label + " produced unexpected length");
        }
        samples.push_back(elapsed);
    }
    return median(samples);
}

static void print_metric(const std::string& metric,
                         const std::string& device,
                         std::size_t n,
                         std::size_t q,
                         double time_us) {
    const double work_items = static_cast<double>(n) * static_cast<double>(q);
    std::cout << metric << ',' << device << ',' << n << ',' << q << ',' << time_us << ','
              << (work_items * 1e6 / time_us) << std::endl;
}

static void create_inputs(ftcl::Interp& interp,
                          const std::string& prefix,
                          const std::string& device,
                          const std::string& points,
                          const std::string& queries) {
    const std::string suffix = device == "cpu" ? "" : " " + device;
    (void)eval_string(interp,
                      "set " + prefix + "_data [geom uvec_points " + points + suffix + "]; "
                      "set " + prefix + "_query [geom uvec_points " + queries + suffix + "]; "
                      "list $" + prefix + "_data $" + prefix + "_query",
                      "create " + prefix + " inputs");
}

static void destroy_inputs(ftcl::Interp& interp, const std::string& prefix) {
    (void)eval_string(interp,
                      "uvec destroy $" + prefix + "_data; uvec destroy $" + prefix + "_query",
                      "destroy " + prefix + " inputs");
}

int main() {
    std::cout << "=== Geometry Performance Scale Smoke Test ===" << std::endl;
    std::cout << "# mode=" << perf_mode()
              << " timing=median_us inputs=prebuilt command=geom" << std::endl;
    auto interp = ftcl::new_interp_with_stdlib();

#ifdef FTCL_ENABLE_CUDA
    (void)eval_ok(interp,
                  "set __warm_p [geom uvec_points {{0 0} {1 1}} cuda:0]; "
                  "set __warm_o [geom batch_distance $__warm_p {0 0} cuda:0]; "
                  "uvec to_list $__warm_o; uvec destroy $__warm_o; uvec destroy $__warm_p",
                  "cuda warmup");
#endif

    std::cout << "metric,device,n,q,time_us,throughput_items_per_s" << std::endl;
    for (const auto& c : scale_cases()) {
        const auto points = points_literal(grid_points(c.n));
        const auto queries = points_literal(grid_points(c.q));

        create_inputs(interp, "cpu", "cpu", points, queries);

        const double cpu_matrix = run_len_median(
            interp,
            "set out [geom batch_distance_matrix $cpu_data $cpu_query]; set n [uvec len $out]; uvec destroy $out; set n",
            "distance matrix cpu scale " + std::to_string(c.n),
            c.n * c.q,
            c.rounds);
        print_metric("batch_distance_matrix", "cpu", c.n, c.q, cpu_matrix);

        const double cpu_nearest = run_len_median(
            interp,
            "set out [geom nearest_point $cpu_data $cpu_query]; set n [uvec len $out]; uvec destroy $out; set n",
            "nearest cpu scale " + std::to_string(c.n),
            c.q * 2,
            c.rounds);
        print_metric("nearest_point", "cpu", c.n, c.q, cpu_nearest);

        const double cpu_range = run_len_median(
            interp,
            "set out [geom range_count_circle $cpu_data $cpu_query 8.0]; set n [uvec len $out]; uvec destroy $out; set n",
            "range cpu scale " + std::to_string(c.n),
            c.q,
            c.rounds);
        print_metric("range_count_circle", "cpu", c.n, c.q, cpu_range);

        destroy_inputs(interp, "cpu");

#ifdef FTCL_ENABLE_CUDA
        create_inputs(interp, "cuda", "cuda:0", points, queries);

        const double cuda_matrix = run_len_median(
            interp,
            "set out [geom batch_distance_matrix $cuda_data $cuda_query cuda:0]; set n [uvec len $out]; uvec destroy $out; set n",
            "distance matrix cuda scale " + std::to_string(c.n),
            c.n * c.q,
            c.rounds);
        print_metric("batch_distance_matrix", "cuda:0", c.n, c.q, cuda_matrix);

        const double cuda_nearest = run_len_median(
            interp,
            "set out [geom nearest_point $cuda_data $cuda_query cuda:0]; set n [uvec len $out]; uvec destroy $out; set n",
            "nearest cuda scale " + std::to_string(c.n),
            c.q * 2,
            c.rounds);
        print_metric("nearest_point", "cuda:0", c.n, c.q, cuda_nearest);

        const double cuda_range = run_len_median(
            interp,
            "set out [geom range_count_circle $cuda_data $cuda_query 8.0 cuda:0]; set n [uvec len $out]; uvec destroy $out; set n",
            "range cuda scale " + std::to_string(c.n),
            c.q,
            c.rounds);
        print_metric("range_count_circle", "cuda:0", c.n, c.q, cuda_range);

        destroy_inputs(interp, "cuda");
#endif
    }

    std::cout << "=== Geometry performance scale smoke test passed ===" << std::endl;
    return 0;
}


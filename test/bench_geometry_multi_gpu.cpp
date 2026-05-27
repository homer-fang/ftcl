#include "geometry_test_utils.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef FTCL_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

using namespace ftcl_geometry_test;

namespace {

struct MultiGpuCase {
    std::size_t n = 4096;
    std::size_t total_q = 512;
    std::size_t rounds = 1;
};

struct MultiGpuResult {
    std::string metric;
    int gpu_count = 0;
    std::size_t n = 0;
    std::size_t total_q = 0;
    std::size_t per_gpu_q = 0;
    std::size_t rounds = 0;
    double time_us = 0.0;
    double total_work_items = 0.0;
    double throughput_items_per_s = 0.0;
};

std::string multi_gpu_mode() {
    const char* mode = std::getenv("FTCL_MULTI_GPU_MODE");
    if (mode == nullptr || std::string(mode).empty()) {
        return "smoke";
    }
    return std::string(mode);
}

std::string scaling_mode() {
    const char* mode = std::getenv("FTCL_MULTI_GPU_SCALING");
    if (mode == nullptr || std::string(mode).empty()) {
        return "strong";
    }
    return std::string(mode);
}

bool weak_scaling() {
    return scaling_mode() == "weak";
}

std::size_t env_size_or(const char* name, std::size_t fallback) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long long value = std::strtoull(raw, &end, 10);
    if (end == raw || *end != '\0' || value == 0) {
        return fallback;
    }
    return static_cast<std::size_t>(value);
}

MultiGpuCase selected_case() {
    MultiGpuCase c;
    if (multi_gpu_mode() == "paper") {
        c.n = 32768;
        c.total_q = 4096;
        c.rounds = 3;
    } else {
        c.n = 4096;
        c.total_q = 512;
        c.rounds = 1;
    }

    c.n = env_size_or("FTCL_MULTI_GPU_N", c.n);
    c.total_q = env_size_or("FTCL_MULTI_GPU_Q", c.total_q);
    c.rounds = env_size_or("FTCL_MULTI_GPU_ROUNDS", c.rounds);
    return c;
}

std::vector<int> requested_gpu_counts(int available) {
    std::vector<int> counts;
    for (int value : {1, 2, 4, 8}) {
        if (value <= available) {
            counts.push_back(value);
        }
    }
    if (counts.empty() && available > 0) {
        counts.push_back(available);
    }
    return counts;
}

std::vector<std::pair<double, double>> grid_points(std::size_t n, std::size_t offset = 0) {
    std::vector<std::pair<double, double>> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t k = i + offset;
        out.emplace_back(static_cast<double>(k % 1009) * 0.25, static_cast<double>((k * 37) % 1013) * 0.25);
    }
    return out;
}

double median(std::vector<double> values) {
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

void expect_handle_len(ftcl::Interp& interp, const std::string& handle, const std::string& label, std::size_t expected_len) {
    const auto value = eval_ok(interp, "uvec len " + handle, label);
    const auto parsed = value.as_int_opt();
    if (!parsed.has_value() || static_cast<std::size_t>(*parsed) != expected_len) {
        fail(label + " produced unexpected length");
    }
}

std::string metric_launch_script(const std::string& metric, int device, std::size_t n, std::size_t per_gpu_q) {
    const std::string suffix = " cuda:" + std::to_string(device);
    if (metric == "batch_distance_matrix") {
        (void)n;
        (void)per_gpu_q;
        return "geom batch_distance_matrix $data $query" + suffix;
    }
    if (metric == "nearest_point") {
        (void)n;
        (void)per_gpu_q;
        return "geom nearest_point $data $query" + suffix;
    }
    if (metric == "range_count_circle") {
        (void)n;
        (void)per_gpu_q;
        return "geom range_count_circle $data $query 8.0" + suffix;
    }
    fail("unknown multi-gpu metric: " + metric);
    return "";
}

std::size_t metric_expected_len(const std::string& metric, std::size_t n, std::size_t per_gpu_q) {
    if (metric == "batch_distance_matrix") {
        return n * per_gpu_q;
    }
    if (metric == "nearest_point") {
        return per_gpu_q * 2;
    }
    if (metric == "range_count_circle") {
        return per_gpu_q;
    }
    fail("unknown multi-gpu metric: " + metric);
    return 0;
}

void run_worker(int device,
                const std::string& metric,
                const std::string& data_literal,
                const std::string& query_literal,
                std::size_t n,
                std::size_t per_gpu_q,
                std::atomic<int>& ready,
                std::atomic<bool>& go,
                std::atomic<int>& done,
                std::atomic<bool>& cleanup_go,
                std::mutex& err_mutex,
                std::exception_ptr& first_error) {
    bool ready_reported = false;
    bool done_reported = false;
    try {
        auto interp = ftcl::new_interp_with_stdlib();
        const std::string suffix = " cuda:" + std::to_string(device);
        (void)eval_ok(interp,
                      "set data [geom uvec_points " + data_literal + suffix + "]; "
                      "set query [geom uvec_points " + query_literal + suffix + "]; "
                      "list $data $query",
                      "multi-gpu create inputs cuda:" + std::to_string(device));

        const std::string script = metric_launch_script(metric, device, n, per_gpu_q);
        const std::size_t expected_len = metric_expected_len(metric, n, per_gpu_q);
        const auto warmup = eval_ok(interp, script, "multi-gpu warmup cuda:" + std::to_string(device));
        const std::string warmup_handle = warmup.as_string();
        expect_handle_len(interp, warmup_handle, "multi-gpu warmup len cuda:" + std::to_string(device), expected_len);
        (void)eval_ok(interp, "uvec destroy " + warmup_handle, "multi-gpu destroy warmup output");

        ready.fetch_add(1, std::memory_order_release);
        ready_reported = true;
        while (!go.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        const auto sample = eval_ok(interp, script, "multi-gpu sample cuda:" + std::to_string(device));
        const std::string sample_handle = sample.as_string();

        done.fetch_add(1, std::memory_order_release);
        done_reported = true;
        while (!cleanup_go.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        expect_handle_len(interp, sample_handle, "multi-gpu sample len cuda:" + std::to_string(device), expected_len);
        (void)eval_ok(interp, "uvec destroy " + sample_handle, "multi-gpu destroy sample output");
        (void)eval_ok(interp, "uvec destroy $data; uvec destroy $query", "multi-gpu destroy inputs");
    } catch (...) {
        std::lock_guard<std::mutex> lock(err_mutex);
        if (first_error == nullptr) {
            first_error = std::current_exception();
        }
        if (!ready_reported) {
            ready.fetch_add(1, std::memory_order_release);
        }
        if (!done_reported) {
            done.fetch_add(1, std::memory_order_release);
        }
    }
}

double run_round(const std::string& metric, int gpu_count, std::size_t n, std::size_t total_q, std::size_t per_gpu_q) {
    const std::string data_literal = points_literal(grid_points(n));
    std::vector<std::string> query_literals;
    query_literals.reserve(static_cast<std::size_t>(gpu_count));
    for (int device = 0; device < gpu_count; ++device) {
        query_literals.push_back(points_literal(grid_points(per_gpu_q, static_cast<std::size_t>(device) * per_gpu_q)));
    }

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::atomic<int> done{0};
    std::atomic<bool> cleanup_go{false};
    std::mutex err_mutex;
    std::exception_ptr first_error = nullptr;
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(gpu_count));

    for (int device = 0; device < gpu_count; ++device) {
        threads.emplace_back(run_worker,
                             device,
                             metric,
                             std::cref(data_literal),
                             std::cref(query_literals[static_cast<std::size_t>(device)]),
                             n,
                             per_gpu_q,
                             std::ref(ready),
                             std::ref(go),
                             std::ref(done),
                             std::ref(cleanup_go),
                             std::ref(err_mutex),
                             std::ref(first_error));
    }

    while (ready.load(std::memory_order_acquire) < gpu_count) {
        std::this_thread::yield();
    }

    if (first_error != nullptr) {
        go.store(true, std::memory_order_release);
        cleanup_go.store(true, std::memory_order_release);
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        std::rethrow_exception(first_error);
    }

    const auto t0 = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    while (done.load(std::memory_order_acquire) < gpu_count) {
        std::this_thread::yield();
    }
    const auto t1 = std::chrono::steady_clock::now();

    cleanup_go.store(true, std::memory_order_release);
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    const double elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(t1 - t0).count();

    if (first_error != nullptr) {
        std::rethrow_exception(first_error);
    }

    (void)total_q;
    return elapsed;
}

MultiGpuResult run_case(const std::string& metric, int gpu_count, const MultiGpuCase& c) {
    const std::size_t per_gpu_q =
        weak_scaling() ? c.total_q : c.total_q / static_cast<std::size_t>(gpu_count);
    if (per_gpu_q == 0) {
        fail("total_q is too small for requested gpu_count");
    }
    const std::size_t actual_total_q =
        weak_scaling() ? per_gpu_q * static_cast<std::size_t>(gpu_count)
                       : per_gpu_q * static_cast<std::size_t>(gpu_count);

    std::vector<double> samples;
    samples.reserve(c.rounds);
    for (std::size_t round = 0; round < c.rounds; ++round) {
        samples.push_back(run_round(metric, gpu_count, c.n, actual_total_q, per_gpu_q));
    }

    MultiGpuResult result;
    result.metric = metric;
    result.gpu_count = gpu_count;
    result.n = c.n;
    result.total_q = actual_total_q;
    result.per_gpu_q = per_gpu_q;
    result.rounds = c.rounds;
    result.time_us = median(samples);
    result.total_work_items = static_cast<double>(c.n) * static_cast<double>(actual_total_q);
    result.throughput_items_per_s = result.total_work_items * 1e6 / result.time_us;
    return result;
}

void print_result(const MultiGpuResult& r, double baseline_us, double baseline_throughput) {
    const double speedup = weak_scaling()
                               ? (baseline_throughput > 0.0 ? r.throughput_items_per_s / baseline_throughput : 0.0)
                               : (baseline_us > 0.0 ? baseline_us / r.time_us : 0.0);
    const double efficiency = r.gpu_count > 0 ? speedup / static_cast<double>(r.gpu_count) : 0.0;
    std::cout << r.metric << ',' << r.gpu_count << ',' << r.n << ',' << r.total_q << ',' << r.per_gpu_q << ','
              << r.rounds << ',' << r.time_us << ',' << r.total_work_items << ',' << r.throughput_items_per_s << ','
              << speedup << ',' << efficiency << std::endl;
}

}  // namespace

int main() {
    std::cout << "=== Geometry Multi-GPU Scaling Benchmark ===" << std::endl;
    std::cout << "# mode=" << multi_gpu_mode()
              << " timing=median_wall_us scaling=" << scaling_mode()
              << " query_partition=round_robin_data_replicated" << std::endl;

#ifndef FTCL_ENABLE_CUDA
    std::cout << "# FTCL_ENABLE_CUDA=OFF; multi-gpu benchmark skipped" << std::endl;
    return 0;
#else
    int device_count = 0;
    const cudaError_t count_err = cudaGetDeviceCount(&device_count);
    if (count_err != cudaSuccess || device_count <= 0) {
        std::cerr << "FAILED: cudaGetDeviceCount: " << cudaGetErrorString(count_err) << std::endl;
        return 1;
    }

    const MultiGpuCase c = selected_case();
    const auto counts = requested_gpu_counts(device_count);
    const std::vector<std::string> metrics = {
        "batch_distance_matrix",
        "nearest_point",
        "range_count_circle",
    };

    std::cout << "# available_gpus=" << device_count << " n=" << c.n << " total_q=" << c.total_q
              << " rounds=" << c.rounds << std::endl;
    std::cout << "metric,gpu_count,n,total_q,per_gpu_q,rounds,time_us,total_work_items,throughput_items_per_s,speedup_vs_1gpu,parallel_efficiency"
              << std::endl;

    for (const auto& metric : metrics) {
        double baseline_us = 0.0;
        double baseline_throughput = 0.0;
        for (const int gpu_count : counts) {
            auto result = run_case(metric, gpu_count, c);
            if (gpu_count == 1) {
                baseline_us = result.time_us;
                baseline_throughput = result.throughput_items_per_s;
            }
            print_result(result, baseline_us, baseline_throughput);
        }
    }

    std::cout << "=== Geometry multi-gpu scaling benchmark passed ===" << std::endl;
    return 0;
#endif
}

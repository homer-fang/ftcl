#include "geometry_test_utils.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace ftcl;
using namespace ftcl_geometry_test;

namespace {

struct ScaleCase {
    std::size_t n;
    std::size_t q;
    std::size_t rounds;
};

struct SummaryStats {
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
};

std::vector<std::pair<double, double>> grid_points(std::size_t n) {
    std::vector<std::pair<double, double>> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.emplace_back(static_cast<double>(i % 1009) * 0.25, static_cast<double>((i * 37) % 1013) * 0.25);
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

double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    if (values.size() == 1) {
        return values.front();
    }
    const double rank = (p / 100.0) * static_cast<double>(values.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(rank);
    const std::size_t hi = std::min(values.size() - 1, lo + 1);
    const double t = rank - static_cast<double>(lo);
    return values[lo] * (1.0 - t) + values[hi] * t;
}

SummaryStats summarize_latency(const std::vector<double>& samples) {
    SummaryStats s{};
    s.p50 = percentile(samples, 50.0);
    s.p95 = percentile(samples, 95.0);
    s.p99 = percentile(samples, 99.0);
    return s;
}

void must_ok(const ftclResult& result, const std::string& where) {
    if (!result.has_value()) {
        throw std::runtime_error(where + ": " + result.error().value().as_string());
    }
}

std::string must_recv(ftclInt channel, const std::string& where) {
    auto result = thread_channel_manager().recv(channel);
    if (!result.has_value()) {
        throw std::runtime_error(where + ": " + result.error().value().as_string());
    }
    return *result;
}

std::string eval_string(Interp& interp, const std::string& script, const std::string& label) {
    return eval_ok(interp, script, label).as_string();
}

double run_median_len(Interp& interp,
                      const std::string& script,
                      const std::string& label,
                      std::size_t expected_len,
                      std::size_t rounds) {
    auto warm = eval_ok(interp, script, label + " warmup");
    auto warm_len = warm.as_int_opt();
    if (!warm_len.has_value() || static_cast<std::size_t>(*warm_len) != expected_len) {
        throw std::runtime_error(label + " warmup output length mismatch");
    }

    std::vector<double> samples;
    samples.reserve(rounds);
    for (std::size_t i = 0; i < rounds; ++i) {
        Value result;
        const double elapsed = timed_us([&]() { result = eval_ok(interp, script, label + " sample " + std::to_string(i)); });
        auto parsed = result.as_int_opt();
        if (!parsed.has_value() || static_cast<std::size_t>(*parsed) != expected_len) {
            throw std::runtime_error(label + " output length mismatch");
        }
        samples.push_back(elapsed);
    }
    return median(samples);
}

std::string metric_script(const std::string& metric,
                          const std::string& data_var,
                          const std::string& query_var,
                          const std::string& device,
                          bool readback) {
    const std::string suffix = device == "cpu" ? "" : (" " + device);

    if (metric == "batch_distance_matrix") {
        if (readback) {
            return "set out [geom batch_distance_matrix $" + data_var + " $" + query_var + suffix +
                   "]; set lst [uvec to_list $out]; set n [llength $lst]; uvec destroy $out; set n";
        }
        return "set out [geom batch_distance_matrix $" + data_var + " $" + query_var + suffix +
               "]; set n [uvec len $out]; uvec destroy $out; set n";
    }

    if (metric == "nearest_point") {
        if (readback) {
            return "set out [geom nearest_point $" + data_var + " $" + query_var + suffix +
                   "]; set lst [uvec to_list $out]; set n [llength $lst]; uvec destroy $out; set n";
        }
        return "set out [geom nearest_point $" + data_var + " $" + query_var + suffix +
               "]; set n [uvec len $out]; uvec destroy $out; set n";
    }

    if (metric == "range_count_circle") {
        if (readback) {
            return "set out [geom range_count_circle $" + data_var + " $" + query_var + " 8.0" + suffix +
                   "]; set lst [uvec to_list $out]; set n [llength $lst]; uvec destroy $out; set n";
        }
        return "set out [geom range_count_circle $" + data_var + " $" + query_var + " 8.0" + suffix +
               "]; set n [uvec len $out]; uvec destroy $out; set n";
    }

    throw std::runtime_error("unknown metric: " + metric);
}

std::size_t expected_len(const std::string& metric, std::size_t n, std::size_t q) {
    if (metric == "batch_distance_matrix") {
        return n * q;
    }
    if (metric == "nearest_point") {
        return q * 2;
    }
    if (metric == "range_count_circle") {
        return q;
    }
    throw std::runtime_error("unknown metric: " + metric);
}

void run_ablation_rows(const ScaleCase& sc, const std::string& metric, const std::string& device) {
    Interp interp = new_interp_with_stdlib();
    const std::string points = points_literal(grid_points(sc.n));
    const std::string queries = points_literal(grid_points(sc.q));
    const std::string suffix = device == "cpu" ? "" : (" " + device);
    const std::size_t expect = expected_len(metric, sc.n, sc.q);

    const std::string inline_script =
        "set data [geom uvec_points " + points + suffix + "]; "
        "set query [geom uvec_points " + queries + suffix + "]; " +
        metric_script(metric, "data", "query", device, false) +
        "; set __keep $n; uvec destroy $query; uvec destroy $data; set __keep";

    const double inline_us = run_median_len(
        interp,
        inline_script,
        metric + " inline " + device,
        expect,
        sc.rounds);
    std::cout << "inline_literal," << metric << ',' << device << ',' << sc.n << ',' << sc.q << ',' << inline_us << '\n';

    (void)eval_string(
        interp,
        "set data [geom uvec_points " + points + suffix + "]; set query [geom uvec_points " + queries + suffix + "]",
        metric + " prebuilt init " + device);

    const double prebuilt_us = run_median_len(
        interp,
        metric_script(metric, "data", "query", device, false),
        metric + " prebuilt " + device,
        expect,
        sc.rounds);
    std::cout << "prebuilt_no_readback," << metric << ',' << device << ',' << sc.n << ',' << sc.q << ',' << prebuilt_us << '\n';

    const double readback_us = run_median_len(
        interp,
        metric_script(metric, "data", "query", device, true),
        metric + " prebuilt readback " + device,
        expect,
        sc.rounds);
    std::cout << "prebuilt_with_readback," << metric << ',' << device << ',' << sc.n << ',' << sc.q << ',' << readback_us << '\n';

    (void)eval_string(interp, "uvec destroy $query; uvec destroy $data", metric + " prebuilt cleanup " + device);
}

std::vector<double> run_concurrency_case(const std::string& device,
                                         std::size_t workers,
                                         std::size_t requests,
                                         std::size_t dataset_n,
                                         std::size_t query_n) {
    auto& mgr = thread_channel_manager();
    const ftclInt req_ch = mgr.create();
    const ftclInt ack_ch = mgr.create();

    std::atomic<bool> failed{false};
    std::mutex err_mu;
    std::string err_msg;

    const std::string points = points_literal(grid_points(dataset_n));
    const std::string queries = points_literal(grid_points(query_n));
    const std::string suffix = device == "cpu" ? "" : (" " + device);
    const std::string step = "set out [geom nearest_point $data $query" + suffix +
                             "]; set n [uvec len $out]; uvec destroy $out; set n";
    const std::string expected = std::to_string(query_n * 2);

    std::vector<std::thread> threads;
    threads.reserve(workers);

    for (std::size_t wid = 0; wid < workers; ++wid) {
        threads.emplace_back([&, wid]() {
            try {
                Interp interp = new_interp_with_stdlib();
                (void)eval_string(
                    interp,
                    "set data [geom uvec_points " + points + suffix + "]; set query [geom uvec_points " + queries + suffix + "]",
                    "worker init " + std::to_string(wid));

                while (true) {
                    const std::string token = must_recv(req_ch, "worker recv " + std::to_string(wid));
                    if (token == "__quit__") {
                        break;
                    }

                    auto step_result = interp.eval(step);
                    if (!step_result.has_value()) {
                        throw std::runtime_error("worker step failed: " + step_result.error().value().as_string());
                    }
                    if (step_result->as_string() != expected) {
                        throw std::runtime_error("worker step length mismatch");
                    }

                    must_ok(thread_channel_manager().send(ack_ch, token), "worker send ack " + std::to_string(wid));
                }

                (void)eval_string(interp, "uvec destroy $query; uvec destroy $data", "worker cleanup " + std::to_string(wid));
            } catch (const std::exception& ex) {
                {
                    std::lock_guard<std::mutex> lock(err_mu);
                    if (!failed.load()) {
                        err_msg = ex.what();
                    }
                }
                failed.store(true);
                (void)thread_channel_manager().send(ack_ch, "__error__");
            }
        });
    }

    const std::size_t warmup = std::min<std::size_t>(64, requests / 4 + 1);
    for (std::size_t i = 0; i < warmup; ++i) {
        const std::string token = "warm_" + std::to_string(i);
        must_ok(mgr.send(req_ch, token), "warm send");
        const std::string ack = must_recv(ack_ch, "warm recv");
        if (ack == "__error__") {
            failed.store(true);
            break;
        }
    }

    if (failed.load()) {
        for (std::size_t i = 0; i < workers; ++i) {
            (void)mgr.send(req_ch, "__quit__");
        }
        for (auto& t : threads) {
            t.join();
        }
        throw std::runtime_error("concurrency warmup failed: " + err_msg);
    }

    const std::size_t window = std::max<std::size_t>(workers * 4, 1);
    std::vector<std::chrono::steady_clock::time_point> send_ts(requests);
    std::vector<double> lat_us;
    lat_us.reserve(requests);

    std::size_t sent = 0;
    std::size_t received = 0;

    const auto t0 = std::chrono::steady_clock::now();
    while (sent < requests && sent < window) {
        send_ts[sent] = std::chrono::steady_clock::now();
        must_ok(mgr.send(req_ch, std::to_string(sent)), "send request");
        ++sent;
    }

    while (received < requests) {
        const std::string token = must_recv(ack_ch, "recv ack");
        if (token == "__error__") {
            failed.store(true);
            break;
        }

        const std::size_t id = static_cast<std::size_t>(std::stoull(token));
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(now - send_ts[id]).count();
        lat_us.push_back(elapsed);
        ++received;

        if (sent < requests) {
            send_ts[sent] = std::chrono::steady_clock::now();
            must_ok(mgr.send(req_ch, std::to_string(sent)), "send request");
            ++sent;
        }
    }
    const auto t1 = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < workers; ++i) {
        must_ok(mgr.send(req_ch, "__quit__"), "send quit");
    }
    for (auto& t : threads) {
        t.join();
    }

    if (failed.load()) {
        throw std::runtime_error("concurrency worker failed: " + err_msg);
    }

    const double elapsed_us = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(t1 - t0).count();
    const double throughput = requests * 1e6 / elapsed_us;
    const SummaryStats s = summarize_latency(lat_us);

    std::cout << device << ',' << workers << ',' << requests << ',' << elapsed_us << ',' << throughput << ',' << s.p50
              << ',' << s.p95 << ',' << s.p99 << '\n';

    return lat_us;
}

}  // namespace

int main() {
    try {
        const ScaleCase ablation_scale{2048, 256, 3};

        std::cout << "[ABLATION]" << std::endl;
        std::cout << "ablation_case,metric,device,n,q,median_us" << std::endl;
        const std::vector<std::string> metrics = {
            "batch_distance_matrix",
            "nearest_point",
            "range_count_circle",
        };

        for (const auto& metric : metrics) {
            run_ablation_rows(ablation_scale, metric, "cpu");
#ifdef FTCL_ENABLE_CUDA
            run_ablation_rows(ablation_scale, metric, "cuda:0");
#endif
        }

        std::cout << "[CONCURRENCY]" << std::endl;
        std::cout << "device,workers,requests,elapsed_us,throughput_req_per_s,p50_us,p95_us,p99_us" << std::endl;

        const std::vector<std::size_t> worker_counts = {1, 2, 4, 8};
        for (std::size_t workers : worker_counts) {
            (void)run_concurrency_case("cpu", workers, 800, 4096, 64);
        }
#ifdef FTCL_ENABLE_CUDA
        for (std::size_t workers : worker_counts) {
            (void)run_concurrency_case("cuda:0", workers, 800, 4096, 64);
        }
#endif

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "bench_geometry_studies failed: " << ex.what() << std::endl;
        return 1;
    }
}


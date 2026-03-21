#include "commands.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace ftcl;

struct SemanticCase {
    std::string script;
    std::string expected;
};

struct SemanticSuite {
    std::string name;
    std::vector<SemanticCase> cases;
};

struct SemanticSuiteResult {
    std::string name;
    std::size_t total = 0;
    std::size_t passed = 0;
};

struct SummaryStats {
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
};

static std::string now_utc_iso8601() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t tt = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

static double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    if (values.size() == 1) {
        return values.front();
    }

    const double rank = (p / 100.0) * static_cast<double>(values.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(rank));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(rank));
    const double t = rank - static_cast<double>(lo);
    return values[lo] * (1.0 - t) + values[hi] * t;
}

static SummaryStats summarize(const std::vector<double>& samples) {
    SummaryStats s{};
    if (samples.empty()) {
        return s;
    }

    const auto [mn_it, mx_it] = std::minmax_element(samples.begin(), samples.end());
    s.min = *mn_it;
    s.max = *mx_it;
    s.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());
    s.p50 = percentile(samples, 50.0);
    s.p95 = percentile(samples, 95.0);
    s.p99 = percentile(samples, 99.0);
    return s;
}

static void ensure_dir(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        throw std::runtime_error("failed to create directory: " + dir.string() + ": " + ec.message());
    }
}

static void write_text(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open file for writing: " + path.string());
    }
    out << content;
}

static std::string csv_escape(std::string s) {
    bool need_quote = false;
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            need_quote = true;
            break;
        }
    }
    if (!need_quote) {
        return s;
    }

    std::string out = "\"";
    for (char c : s) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

static bool eval_expect(const std::string& script, const std::string& expected) {
    auto interp = new_interp_with_stdlib();
    auto result = interp.eval(script);
    if (!result.has_value()) {
        return false;
    }
    return result->as_string() == expected;
}

static std::vector<SemanticSuiteResult> run_semantic_pass_rate() {
    std::vector<SemanticSuite> suites = {
        {
            "core",
            {
                {"set x 1; incr x; set x", "2"},
                {"set s hello; append s _world; set s", "hello_world"},
                {"catch {error boom} msg; set msg", "boom"},
                {"set x 10; unset x; catch {set x} m; set m", "can't read \"x\": no such variable"},
            },
        },
        {
            "collections",
            {
                {"lindex [list a b c] 1", "b"},
                {"llength [list 1 2 3 4]", "4"},
                {"dict get [dict create a 1 b 2] b", "2"},
                {"string length abcdef", "6"},
                {"string toupper abc", "ABC"},
            },
        },
        {
            "control_flow",
            {
                {"set sum 0; for {set i 0} {$i < 5} {incr i} {incr sum $i}; set sum", "10"},
                {"set out 0; if {1} {set out 7} else {set out 9}; set out", "7"},
                {"set acc {}; foreach x {a b c} {append acc $x}; set acc", "abc"},
                {"set i 0; while {$i < 3} {incr i}; set i", "3"},
            },
        },
        {
            "expr",
            {
                {"expr {1 + 2 * 3}", "7"},
                {"expr {(10 > 3) && (2 < 5)}", "1"},
                {"expr {5 % 2}", "1"},
                {"expr {1 ? 11 : 22}", "11"},
                {"expr {(1 << 4) + (16 >> 2)}", "20"},
            },
        },
        {
            "threading",
            {
                {"set id [thread spawn {expr {6*7}}]; thread await $id", "42"},
                {"set ch [thread channel create]; thread channel send $ch ping; thread channel recv $ch", "ping"},
                {"set ch [thread channel create]; set v [thread channel try_recv $ch]; string length $v", "0"},
                {"sleep 1; set done ok", "ok"},
            },
        },
    };

    std::vector<SemanticSuiteResult> results;
    results.reserve(suites.size());

    for (const auto& suite : suites) {
        SemanticSuiteResult r{};
        r.name = suite.name;
        r.total = suite.cases.size();
        r.passed = 0;
        for (const auto& c : suite.cases) {
            if (eval_expect(c.script, c.expected)) {
                ++r.passed;
            }
        }
        results.push_back(r);
    }

    return results;
}

static void write_semantic_csv(const fs::path& out_dir, const std::vector<SemanticSuiteResult>& suites) {
    std::ostringstream suites_csv;
    suites_csv << "suite,total,passed,pass_rate_pct\n";

    std::size_t total = 0;
    std::size_t passed = 0;
    for (const auto& s : suites) {
        total += s.total;
        passed += s.passed;
        const double rate = s.total == 0 ? 0.0 : 100.0 * static_cast<double>(s.passed) / static_cast<double>(s.total);
        suites_csv << csv_escape(s.name) << ',' << s.total << ',' << s.passed << ','
                   << std::fixed << std::setprecision(2) << rate << '\n';
    }

    write_text(out_dir / "semantic_pass_rate.csv", suites_csv.str());

    const double overall = total == 0 ? 0.0 : 100.0 * static_cast<double>(passed) / static_cast<double>(total);
    std::ostringstream snap;
    snap << "timestamp_utc,total,passed,pass_rate_pct\n";
    snap << now_utc_iso8601() << ',' << total << ',' << passed << ',' << std::fixed << std::setprecision(2) << overall
         << '\n';
    write_text(out_dir / "semantic_pass_rate_snapshot.csv", snap.str());
}

static void must_ok(const ftclResult& r, const std::string& where) {
    if (!r.has_value()) {
        throw std::runtime_error(where + ": " + r.error().value().as_string());
    }
}

static std::string must_recv(ftclInt channel, const std::string& where) {
    auto r = thread_channel_manager().recv(channel);
    if (!r.has_value()) {
        throw std::runtime_error(where + ": " + r.error().value().as_string());
    }
    return *r;
}

static std::vector<double> run_channel_latency_benchmark(std::size_t warmup, std::size_t samples) {
    auto& mgr = thread_channel_manager();
    const ftclInt req = mgr.create();
    const ftclInt ack = mgr.create();

    const std::size_t total = warmup + samples;
    std::thread worker([req, ack, total]() {
        for (std::size_t i = 0; i < total; ++i) {
            const std::string payload = must_recv(req, "worker recv");
            must_ok(thread_channel_manager().send(ack, payload), "worker send");
        }
    });

    std::vector<double> one_way_us;
    one_way_us.reserve(samples);

    for (std::size_t i = 0; i < total; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        must_ok(mgr.send(req, "x"), "main send");
        (void)must_recv(ack, "main recv");
        const auto t1 = std::chrono::steady_clock::now();
        if (i >= warmup) {
            const auto rtt_us =
                std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(t1 - t0).count();
            one_way_us.push_back(rtt_us / 2.0);
        }
    }

    worker.join();
    return one_way_us;
}

static std::vector<double> run_frame_time_benchmark(std::size_t warmup, std::size_t samples) {
    auto interp = new_interp_with_stdlib();

    const std::string init_script = R"(
set width 27
set height 17
set px 2
set py 14
set dx 1
set dy 0
set e1x 24
set e1y 2
set e2x 23
set e2y 14
set bullets {}
proc frame_step {} {
    global width height px py dx dy e1x e1y e2x e2y bullets
    set i 0
    while {$i < 24} {
        set px [expr {($px + $dx + $width) % $width}]
        set py [expr {($py + $dy + $height) % $height}]

        if {[llength $bullets] < 24} {
            lappend bullets [list $px $py 1 0 8]
        }

        set next {}
        foreach b $bullets {
            set bx [expr {[lindex $b 0] + [lindex $b 2]}]
            set by [expr {[lindex $b 1] + [lindex $b 3]}]
            set life [expr {[lindex $b 4] - 1}]
            if {$life >= 0 && $bx > 0 && $bx < ($width - 1) && $by > 0 && $by < ($height - 1)} {
                lappend next [list $bx $by [lindex $b 2] [lindex $b 3] $life]
            }
        }
        set bullets $next

        set e1x [expr {($e1x + 1) % ($width - 1)}]
        set e2y [expr {($e2y + 1) % ($height - 1)}]
        incr i
    }
    return [llength $bullets]
}
)";

    auto init = interp.eval(init_script);
    if (!init.has_value()) {
        throw std::runtime_error("frame benchmark init failed: " + init.error().value().as_string());
    }

    std::vector<double> frame_us;
    frame_us.reserve(samples);
    const std::size_t total = warmup + samples;
    for (std::size_t i = 0; i < total; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        auto step = interp.eval("frame_step");
        const auto t1 = std::chrono::steady_clock::now();
        if (!step.has_value()) {
            throw std::runtime_error("frame benchmark step failed: " + step.error().value().as_string());
        }
        if (i >= warmup) {
            frame_us.push_back(
                std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(t1 - t0).count());
        }
    }

    return frame_us;
}

static void write_samples_csv(const fs::path& path, const std::string& metric_name, const std::vector<double>& values) {
    std::ostringstream out;
    out << "sample_idx," << metric_name << "\n";
    for (std::size_t i = 0; i < values.size(); ++i) {
        out << i << ',' << std::fixed << std::setprecision(3) << values[i] << '\n';
    }
    write_text(path, out.str());
}

static void write_summary_csv(const fs::path& path, const std::string& metric_name, const std::vector<double>& values) {
    const SummaryStats s = summarize(values);
    std::ostringstream out;
    out << "metric,samples,min_us,max_us,mean_us,p50_us,p95_us,p99_us\n";
    out << metric_name << ',' << values.size() << ',' << std::fixed << std::setprecision(3) << s.min << ',' << s.max
        << ',' << s.mean << ',' << s.p50 << ',' << s.p95 << ',' << s.p99 << '\n';
    write_text(path, out.str());
}

int main(int argc, char** argv) {
    try {
        const fs::path out_dir =
            (argc >= 2) ? fs::path(argv[1]) : fs::path("benchmark_out");
        ensure_dir(out_dir);

        const std::size_t warmup = 500;
        const std::size_t samples = 5000;

        const auto suites = run_semantic_pass_rate();
        write_semantic_csv(out_dir, suites);

        const auto channel_us = run_channel_latency_benchmark(warmup, samples);
        write_samples_csv(out_dir / "channel_latency_us.csv", "one_way_latency_us", channel_us);
        write_summary_csv(out_dir / "channel_latency_summary.csv", "channel_one_way_latency", channel_us);

        const auto frame_us = run_frame_time_benchmark(warmup, samples);
        write_samples_csv(out_dir / "frame_time_us.csv", "frame_time_us", frame_us);
        write_summary_csv(out_dir / "frame_time_summary.csv", "frame_time", frame_us);

        const auto c = summarize(channel_us);
        const auto f = summarize(frame_us);

        std::cout << "bench_ftcl output: " << out_dir.string() << "\n";
        std::cout << "channel latency (one-way, us): p50=" << std::fixed << std::setprecision(3) << c.p50
                  << " p95=" << c.p95 << " p99=" << c.p99 << "\n";
        std::cout << "frame time (us): p50=" << f.p50 << " p95=" << f.p95 << " p99=" << f.p99 << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "bench_ftcl failed: " << ex.what() << std::endl;
        return 1;
    }
}


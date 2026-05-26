#pragma once

#include "commands.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ftcl_geometry_test {

inline void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << std::endl;
    std::exit(1);
}

inline void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
    std::cout << "  [OK] " << message << std::endl;
}

inline void assert_eq(const std::string& actual, const std::string& expected, const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAILED: " << message << std::endl;
        std::cerr << "  expected: [" << expected << "]" << std::endl;
        std::cerr << "  actual:   [" << actual << "]" << std::endl;
        std::exit(1);
    }
    std::cout << "  [OK] " << message << std::endl;
}

inline ftcl::Value eval_ok(ftcl::Interp& interp, const std::string& script, const std::string& label) {
    auto result = interp.eval(script);
    if (!result.has_value()) {
        std::cerr << "FAILED: " << label << std::endl;
        std::cerr << "  script: " << script << std::endl;
        std::cerr << "  error: " << result.error().value().as_string() << std::endl;
        std::exit(1);
    }
    return *result;
}

inline std::vector<double> value_to_numbers(const ftcl::Value& value, const std::string& label) {
    auto list = value.as_list();
    if (!list.has_value()) {
        auto single = value.as_float_opt();
        if (single.has_value()) {
            return {*single};
        }
        std::cerr << "FAILED: " << label << std::endl;
        std::cerr << "  cannot parse list: " << list.error() << std::endl;
        std::exit(1);
    }

    std::vector<double> out;
    out.reserve(list->size());
    for (const auto& item : *list) {
        auto parsed = item.as_float_opt();
        if (!parsed.has_value()) {
            std::cerr << "FAILED: " << label << std::endl;
            std::cerr << "  non-numeric item: " << item.as_string() << std::endl;
            std::exit(1);
        }
        out.push_back(*parsed);
    }
    return out;
}

inline std::vector<double> eval_numbers(ftcl::Interp& interp, const std::string& script, const std::string& label) {
    return value_to_numbers(eval_ok(interp, script, label), label);
}

inline void assert_close(double actual, double expected, double eps, const std::string& message) {
    if (std::fabs(actual - expected) > eps) {
        std::cerr << "FAILED: " << message << std::endl;
        std::cerr << std::setprecision(17) << "  expected: " << expected << std::endl;
        std::cerr << std::setprecision(17) << "  actual:   " << actual << std::endl;
        std::exit(1);
    }
    std::cout << "  [OK] " << message << std::endl;
}

inline void assert_close_vec(const std::vector<double>& actual,
                             const std::vector<double>& expected,
                             double eps,
                             const std::string& message) {
    if (actual.size() != expected.size()) {
        std::cerr << "FAILED: " << message << std::endl;
        std::cerr << "  expected size: " << expected.size() << std::endl;
        std::cerr << "  actual size:   " << actual.size() << std::endl;
        std::exit(1);
    }
    double max_err = 0.0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const double err = std::fabs(actual[i] - expected[i]);
        max_err = std::max(max_err, err);
        if (err > eps) {
            std::cerr << "FAILED: " << message << std::endl;
            std::cerr << "  index: " << i << std::endl;
            std::cerr << std::setprecision(17) << "  expected: " << expected[i] << std::endl;
            std::cerr << std::setprecision(17) << "  actual:   " << actual[i] << std::endl;
            std::cerr << std::setprecision(17) << "  max_err:  " << max_err << std::endl;
            std::exit(1);
        }
    }
    std::cout << "  [OK] " << message << " max_err=" << std::setprecision(6) << max_err << std::endl;
}

inline std::string num(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << value;
    return out.str();
}

inline std::string points_literal(const std::vector<std::pair<double, double>>& points) {
    std::ostringstream out;
    out << "{";
    for (const auto& [x, y] : points) {
        out << " {" << num(x) << ' ' << num(y) << "}";
    }
    out << "}";
    return out.str();
}

inline std::string segments_literal(const std::vector<std::array<double, 4>>& segments) {
    std::ostringstream out;
    out << "{";
    for (const auto& s : segments) {
        out << " {{" << num(s[0]) << ' ' << num(s[1]) << "} {" << num(s[2]) << ' ' << num(s[3]) << "}}";
    }
    out << "}";
    return out.str();
}

inline std::string aabbs_literal(const std::vector<std::array<double, 4>>& boxes) {
    std::ostringstream out;
    out << "{";
    for (const auto& b : boxes) {
        out << " {{" << num(b[0]) << ' ' << num(b[1]) << "} {" << num(b[2]) << ' ' << num(b[3]) << "}}";
    }
    out << "}";
    return out.str();
}

inline std::string values_literal(const std::vector<double>& values) {
    std::ostringstream out;
    out << "{";
    for (double value : values) {
        out << ' ' << num(value);
    }
    out << "}";
    return out.str();
}

inline std::vector<std::pair<double, double>> random_points(std::mt19937& rng,
                                                            std::size_t count,
                                                            double lo = -20.0,
                                                            double hi = 20.0) {
    std::uniform_real_distribution<double> dist(lo, hi);
    std::vector<std::pair<double, double>> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.emplace_back(dist(rng), dist(rng));
    }
    return out;
}

inline std::vector<std::array<double, 4>> random_segments(std::mt19937& rng, std::size_t count) {
    std::uniform_real_distribution<double> dist(-20.0, 20.0);
    std::vector<std::array<double, 4>> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back({dist(rng), dist(rng), dist(rng), dist(rng)});
    }
    return out;
}

inline std::vector<std::array<double, 4>> random_aabbs(std::mt19937& rng, std::size_t count) {
    std::uniform_real_distribution<double> dist(-20.0, 20.0);
    std::uniform_real_distribution<double> size_dist(0.1, 8.0);
    std::vector<std::array<double, 4>> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double x = dist(rng);
        const double y = dist(rng);
        out.push_back({x, y, x + size_dist(rng), y + size_dist(rng)});
    }
    return out;
}

inline double timed_us(const std::function<void()>& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(t1 - t0).count();
}

}  // namespace ftcl_geometry_test

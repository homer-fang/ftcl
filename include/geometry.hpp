#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace ftcl::geom {

inline constexpr double kEps = 1e-9;

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct Segment {
    Point a;
    Point b;
};

struct BoundingBox {
    Point min;
    Point max;
};

enum class PointLocation {
    Outside,
    Boundary,
    Inside,
};

inline int sign(double value, double eps = kEps) {
    if (value > eps) {
        return 1;
    }
    if (value < -eps) {
        return -1;
    }
    return 0;
}

inline bool almost_equal(double lhs, double rhs, double eps = kEps) {
    return std::fabs(lhs - rhs) <= eps;
}

inline bool same_point(const Point& lhs, const Point& rhs, double eps = kEps) {
    return almost_equal(lhs.x, rhs.x, eps) && almost_equal(lhs.y, rhs.y, eps);
}

inline Point operator+(const Point& lhs, const Point& rhs) {
    return Point{lhs.x + rhs.x, lhs.y + rhs.y};
}

inline Point operator-(const Point& lhs, const Point& rhs) {
    return Point{lhs.x - rhs.x, lhs.y - rhs.y};
}

inline Point operator*(const Point& p, double scale) {
    return Point{p.x * scale, p.y * scale};
}

inline double dot(const Point& lhs, const Point& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline double cross(const Point& lhs, const Point& rhs) {
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

inline double cross(const Point& origin, const Point& a, const Point& b) {
    return cross(a - origin, b - origin);
}

inline double distance2(const Point& lhs, const Point& rhs) {
    const double dx = lhs.x - rhs.x;
    const double dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

inline double distance(const Point& lhs, const Point& rhs) {
    return std::sqrt(distance2(lhs, rhs));
}

inline int orientation(const Point& a, const Point& b, const Point& c) {
    return sign(cross(a, b, c));
}

inline bool on_segment(const Point& a, const Point& b, const Point& p) {
    if (orientation(a, b, p) != 0) {
        return false;
    }

    return p.x + kEps >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) + kEps &&
           p.y + kEps >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y) + kEps;
}

inline bool segments_intersect(const Point& a, const Point& b, const Point& c, const Point& d) {
    const int o1 = orientation(a, b, c);
    const int o2 = orientation(a, b, d);
    const int o3 = orientation(c, d, a);
    const int o4 = orientation(c, d, b);

    if (o1 == 0 && on_segment(a, b, c)) {
        return true;
    }
    if (o2 == 0 && on_segment(a, b, d)) {
        return true;
    }
    if (o3 == 0 && on_segment(c, d, a)) {
        return true;
    }
    if (o4 == 0 && on_segment(c, d, b)) {
        return true;
    }

    return o1 != o2 && o3 != o4;
}

inline std::optional<Point> line_intersection(const Point& a, const Point& b, const Point& c, const Point& d) {
    const Point r = b - a;
    const Point s = d - c;
    const double denom = cross(r, s);
    if (sign(denom) == 0) {
        return std::nullopt;
    }

    const double t = cross(c - a, s) / denom;
    return a + r * t;
}

inline double point_line_distance(const Point& p, const Point& a, const Point& b) {
    const double len = distance(a, b);
    if (len <= kEps) {
        return distance(p, a);
    }
    return std::fabs(cross(b - a, p - a)) / len;
}

inline double point_segment_distance(const Point& p, const Point& a, const Point& b) {
    const Point ab = b - a;
    const double len2 = dot(ab, ab);
    if (len2 <= kEps) {
        return distance(p, a);
    }

    const double t = std::max(0.0, std::min(1.0, dot(p - a, ab) / len2));
    return distance(p, a + ab * t);
}

inline double polygon_signed_area(const std::vector<Point>& polygon) {
    if (polygon.size() < 3) {
        return 0.0;
    }

    double twice_area = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Point& a = polygon[i];
        const Point& b = polygon[(i + 1) % polygon.size()];
        twice_area += cross(a, b);
    }
    return twice_area / 2.0;
}

inline double polygon_area(const std::vector<Point>& polygon) {
    return std::fabs(polygon_signed_area(polygon));
}

inline double polygon_perimeter(const std::vector<Point>& polygon) {
    if (polygon.size() < 2) {
        return 0.0;
    }

    double total = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        total += distance(polygon[i], polygon[(i + 1) % polygon.size()]);
    }
    return total;
}

inline PointLocation point_in_polygon(const Point& p, const std::vector<Point>& polygon) {
    if (polygon.size() < 3) {
        return PointLocation::Outside;
    }

    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Point& a = polygon[j];
        const Point& b = polygon[i];

        if (on_segment(a, b, p)) {
            return PointLocation::Boundary;
        }

        const bool crosses_y = (a.y > p.y) != (b.y > p.y);
        if (!crosses_y) {
            continue;
        }

        const double x_at_y = (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x;
        if (p.x < x_at_y) {
            inside = !inside;
        }
    }

    return inside ? PointLocation::Inside : PointLocation::Outside;
}

inline std::string point_location_to_string(PointLocation location) {
    switch (location) {
        case PointLocation::Outside:
            return "outside";
        case PointLocation::Boundary:
            return "boundary";
        case PointLocation::Inside:
            return "inside";
    }
    return "outside";
}

inline std::vector<Point> convex_hull(std::vector<Point> points) {
    std::sort(points.begin(), points.end(), [](const Point& lhs, const Point& rhs) {
        if (lhs.x != rhs.x) {
            return lhs.x < rhs.x;
        }
        return lhs.y < rhs.y;
    });

    points.erase(std::unique(points.begin(), points.end(), [](const Point& lhs, const Point& rhs) { return same_point(lhs, rhs); }), points.end());
    if (points.size() <= 1) {
        return points;
    }

    std::vector<Point> lower;
    for (const auto& p : points) {
        while (lower.size() >= 2 && cross(lower[lower.size() - 2], lower.back(), p) <= kEps) {
            lower.pop_back();
        }
        lower.push_back(p);
    }

    std::vector<Point> upper;
    for (auto it = points.rbegin(); it != points.rend(); ++it) {
        while (upper.size() >= 2 && cross(upper[upper.size() - 2], upper.back(), *it) <= kEps) {
            upper.pop_back();
        }
        upper.push_back(*it);
    }

    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

inline std::optional<BoundingBox> bounding_box(const std::vector<Point>& points) {
    if (points.empty()) {
        return std::nullopt;
    }

    BoundingBox box{points.front(), points.front()};
    for (const auto& p : points) {
        box.min.x = std::min(box.min.x, p.x);
        box.min.y = std::min(box.min.y, p.y);
        box.max.x = std::max(box.max.x, p.x);
        box.max.y = std::max(box.max.y, p.y);
    }
    return box;
}

inline std::optional<double> closest_pair_distance(const std::vector<Point>& points) {
    if (points.size() < 2) {
        return std::nullopt;
    }

    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < points.size(); ++i) {
        for (std::size_t j = i + 1; j < points.size(); ++j) {
            best = std::min(best, distance(points[i], points[j]));
        }
    }
    return best;
}

}  // namespace ftcl::geom

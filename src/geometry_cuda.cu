#include "geometry_cuda.hpp"

#ifdef FTCL_GEOMETRY_CUDA

#include <cuda_runtime.h>

#include <string>

namespace ftcl::geom {
namespace {

constexpr ftclFloat kDeviceEps = 1e-9;
constexpr int kThreads = 256;

inline std::string cuda_error(const char* op, cudaError_t err) {
    return std::string(op) + " failed: " + cudaGetErrorString(err);
}

inline ftcl::expected<std::size_t, std::string> prepare_cuda_binary(RawUMutPtr<ftclFloat> dst,
                                                                    RawUPtr<ftclFloat> lhs,
                                                                    RawUPtr<ftclFloat> rhs,
                                                                    const char* name) {
    if (!dst.device().is_cuda() || !lhs.device().is_cuda() || !rhs.device().is_cuda()) {
        return ftcl::unexpected(std::string(name) + " requires CUDA pointers");
    }
    if (dst.device() != lhs.device() || dst.device() != rhs.device()) {
        return ftcl::unexpected(std::string(name) + " requires all pointers on the same CUDA device");
    }
    const auto set_device = cudaSetDevice(dst.device().index());
    if (set_device != cudaSuccess) {
        return ftcl::unexpected(cuda_error("cudaSetDevice", set_device));
    }
    return static_cast<std::size_t>(0);
}

inline ftcl::expected<std::size_t, std::string> prepare_cuda_unary(RawUMutPtr<ftclFloat> dst,
                                                                   RawUPtr<ftclFloat> src,
                                                                   const char* name) {
    if (!dst.device().is_cuda() || !src.device().is_cuda()) {
        return ftcl::unexpected(std::string(name) + " requires CUDA pointers");
    }
    if (dst.device() != src.device()) {
        return ftcl::unexpected(std::string(name) + " requires all pointers on the same CUDA device");
    }
    const auto set_device = cudaSetDevice(dst.device().index());
    if (set_device != cudaSuccess) {
        return ftcl::unexpected(cuda_error("cudaSetDevice", set_device));
    }
    return static_cast<std::size_t>(0);
}

inline ftcl::expected<std::size_t, std::string> finish_kernel(const char* name, std::size_t count) {
    const auto launch = cudaGetLastError();
    if (launch != cudaSuccess) {
        return ftcl::unexpected(cuda_error(name, launch));
    }
    const auto sync = cudaDeviceSynchronize();
    if (sync != cudaSuccess) {
        return ftcl::unexpected(cuda_error(name, sync));
    }
    return count;
}

__device__ int dsign(ftclFloat value) {
    if (value > kDeviceEps) {
        return 1;
    }
    if (value < -kDeviceEps) {
        return -1;
    }
    return 0;
}

__device__ ftclFloat dcross(ftclFloat ax, ftclFloat ay, ftclFloat bx, ftclFloat by) {
    return ax * by - ay * bx;
}

__device__ ftclFloat ddist(ftclFloat ax, ftclFloat ay, ftclFloat bx, ftclFloat by) {
    const ftclFloat dx = ax - bx;
    const ftclFloat dy = ay - by;
    return sqrt(dx * dx + dy * dy);
}

__device__ ftclFloat dpoint_segment_distance(ftclFloat px,
                                             ftclFloat py,
                                             ftclFloat ax,
                                             ftclFloat ay,
                                             ftclFloat bx,
                                             ftclFloat by) {
    const ftclFloat abx = bx - ax;
    const ftclFloat aby = by - ay;
    const ftclFloat len2 = abx * abx + aby * aby;
    if (len2 <= kDeviceEps) {
        return ddist(px, py, ax, ay);
    }
    ftclFloat t = ((px - ax) * abx + (py - ay) * aby) / len2;
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }
    return ddist(px, py, ax + abx * t, ay + aby * t);
}

__device__ int dorientation(ftclFloat ax, ftclFloat ay, ftclFloat bx, ftclFloat by, ftclFloat cx, ftclFloat cy) {
    return dsign(dcross(bx - ax, by - ay, cx - ax, cy - ay));
}

__device__ bool don_segment(ftclFloat ax, ftclFloat ay, ftclFloat bx, ftclFloat by, ftclFloat px, ftclFloat py) {
    if (dorientation(ax, ay, bx, by, px, py) != 0) {
        return false;
    }
    const ftclFloat minx = ax < bx ? ax : bx;
    const ftclFloat maxx = ax > bx ? ax : bx;
    const ftclFloat miny = ay < by ? ay : by;
    const ftclFloat maxy = ay > by ? ay : by;
    return px + kDeviceEps >= minx && px <= maxx + kDeviceEps && py + kDeviceEps >= miny && py <= maxy + kDeviceEps;
}

__device__ bool dsegments_intersect(ftclFloat ax,
                                    ftclFloat ay,
                                    ftclFloat bx,
                                    ftclFloat by,
                                    ftclFloat cx,
                                    ftclFloat cy,
                                    ftclFloat dx,
                                    ftclFloat dy) {
    const int o1 = dorientation(ax, ay, bx, by, cx, cy);
    const int o2 = dorientation(ax, ay, bx, by, dx, dy);
    const int o3 = dorientation(cx, cy, dx, dy, ax, ay);
    const int o4 = dorientation(cx, cy, dx, dy, bx, by);
    if (o1 == 0 && don_segment(ax, ay, bx, by, cx, cy)) return true;
    if (o2 == 0 && don_segment(ax, ay, bx, by, dx, dy)) return true;
    if (o3 == 0 && don_segment(cx, cy, dx, dy, ax, ay)) return true;
    if (o4 == 0 && don_segment(cx, cy, dx, dy, bx, by)) return true;
    return o1 != o2 && o3 != o4;
}

__device__ int dpoint_in_polygon(ftclFloat px, ftclFloat py, const ftclFloat* polygon, std::size_t polygon_count) {
    if (polygon_count < 3) {
        return 0;
    }
    bool inside = false;
    for (std::size_t i = 0, j = polygon_count - 1; i < polygon_count; j = i++) {
        const ftclFloat ax = polygon[j * 2];
        const ftclFloat ay = polygon[j * 2 + 1];
        const ftclFloat bx = polygon[i * 2];
        const ftclFloat by = polygon[i * 2 + 1];
        if (don_segment(ax, ay, bx, by, px, py)) {
            return 1;
        }
        const bool crosses_y = (ay > py) != (by > py);
        if (!crosses_y) {
            continue;
        }
        const ftclFloat x_at_y = (bx - ax) * (py - ay) / (by - ay) + ax;
        if (px < x_at_y) {
            inside = !inside;
        }
    }
    return inside ? 2 : 0;
}

__global__ void batch_distance_kernel(ftclFloat* dst,
                                      const ftclFloat* points,
                                      ftclFloat qx,
                                      ftclFloat qy,
                                      std::size_t point_count) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= point_count) return;
    dst[i] = ddist(points[i * 2], points[i * 2 + 1], qx, qy);
}

__global__ void batch_distance_matrix_kernel(ftclFloat* dst,
                                             const ftclFloat* lhs,
                                             const ftclFloat* rhs,
                                             std::size_t lhs_count,
                                             std::size_t rhs_count) {
    const std::size_t idx = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    const std::size_t total = lhs_count * rhs_count;
    if (idx >= total) return;
    const std::size_t i = idx / rhs_count;
    const std::size_t j = idx % rhs_count;
    dst[idx] = ddist(lhs[i * 2], lhs[i * 2 + 1], rhs[j * 2], rhs[j * 2 + 1]);
}

__global__ void batch_point_in_polygon_kernel(ftclFloat* dst,
                                              const ftclFloat* points,
                                              const ftclFloat* polygon,
                                              std::size_t point_count,
                                              std::size_t polygon_count) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= point_count) return;
    dst[i] = static_cast<ftclFloat>(dpoint_in_polygon(points[i * 2], points[i * 2 + 1], polygon, polygon_count));
}

__global__ void batch_segment_intersect_kernel(ftclFloat* dst,
                                               const ftclFloat* lhs,
                                               const ftclFloat* rhs,
                                               std::size_t segment_count) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= segment_count) return;
    const std::size_t a = i * 4;
    dst[i] = dsegments_intersect(lhs[a], lhs[a + 1], lhs[a + 2], lhs[a + 3], rhs[a], rhs[a + 1], rhs[a + 2], rhs[a + 3])
                 ? 1.0
                 : 0.0;
}

__global__ void batch_point_segment_distance_kernel(ftclFloat* dst,
                                                    const ftclFloat* points,
                                                    const ftclFloat* segments,
                                                    std::size_t pair_count) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= pair_count) return;
    const std::size_t p = i * 2;
    const std::size_t s = i * 4;
    dst[i] = dpoint_segment_distance(points[p], points[p + 1], segments[s], segments[s + 1], segments[s + 2], segments[s + 3]);
}

__global__ void nearest_point_kernel(ftclFloat* dst,
                                     const ftclFloat* dataset,
                                     const ftclFloat* queries,
                                     std::size_t dataset_count,
                                     std::size_t query_count) {
    const std::size_t qi = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (qi >= query_count) return;
    ftclFloat best = 1.0e300;
    std::size_t best_index = 0;
    const ftclFloat qx = queries[qi * 2];
    const ftclFloat qy = queries[qi * 2 + 1];
    for (std::size_t di = 0; di < dataset_count; ++di) {
        const ftclFloat d = ddist(dataset[di * 2], dataset[di * 2 + 1], qx, qy);
        if (d < best) {
            best = d;
            best_index = di;
        }
    }
    dst[qi * 2] = static_cast<ftclFloat>(best_index);
    dst[qi * 2 + 1] = best;
}

__global__ void k_nearest_points_kernel(ftclFloat* dst,
                                        const ftclFloat* dataset,
                                        const ftclFloat* queries,
                                        std::size_t dataset_count,
                                        std::size_t query_count,
                                        std::size_t k) {
    const std::size_t qi = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (qi >= query_count) return;
    const ftclFloat qx = queries[qi * 2];
    const ftclFloat qy = queries[qi * 2 + 1];
    const std::size_t base = qi * k * 2;
    for (std::size_t slot = 0; slot < k; ++slot) {
        dst[base + slot * 2] = -1.0;
        dst[base + slot * 2 + 1] = 1.0e300;
    }
    for (std::size_t di = 0; di < dataset_count; ++di) {
        const ftclFloat d = ddist(dataset[di * 2], dataset[di * 2 + 1], qx, qy);
        for (std::size_t slot = 0; slot < k; ++slot) {
            const std::size_t pos = base + slot * 2;
            if (d < dst[pos + 1]) {
                for (std::size_t shift = k - 1; shift > slot; --shift) {
                    dst[base + shift * 2] = dst[base + (shift - 1) * 2];
                    dst[base + shift * 2 + 1] = dst[base + (shift - 1) * 2 + 1];
                }
                dst[pos] = static_cast<ftclFloat>(di);
                dst[pos + 1] = d;
                break;
            }
        }
    }
}

__global__ void range_count_circle_kernel(ftclFloat* dst,
                                          const ftclFloat* points,
                                          const ftclFloat* centers,
                                          std::size_t point_count,
                                          std::size_t center_count,
                                          ftclFloat radius) {
    const std::size_t ci = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (ci >= center_count) return;
    const ftclFloat cx = centers[ci * 2];
    const ftclFloat cy = centers[ci * 2 + 1];
    const ftclFloat r2 = radius * radius;
    std::size_t count = 0;
    for (std::size_t pi = 0; pi < point_count; ++pi) {
        const ftclFloat dx = points[pi * 2] - cx;
        const ftclFloat dy = points[pi * 2 + 1] - cy;
        if (dx * dx + dy * dy <= r2 + kDeviceEps) ++count;
    }
    dst[ci] = static_cast<ftclFloat>(count);
}

__global__ void range_count_rect_kernel(ftclFloat* dst,
                                        const ftclFloat* points,
                                        const ftclFloat* rects,
                                        std::size_t point_count,
                                        std::size_t rect_count) {
    const std::size_t ri = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (ri >= rect_count) return;
    const std::size_t r = ri * 4;
    std::size_t count = 0;
    for (std::size_t pi = 0; pi < point_count; ++pi) {
        const ftclFloat x = points[pi * 2];
        const ftclFloat y = points[pi * 2 + 1];
        if (x + kDeviceEps >= rects[r] && x <= rects[r + 2] + kDeviceEps && y + kDeviceEps >= rects[r + 1] &&
            y <= rects[r + 3] + kDeviceEps) {
            ++count;
        }
    }
    dst[ri] = static_cast<ftclFloat>(count);
}

__global__ void bbox_reduce_kernel(ftclFloat* dst, const ftclFloat* points, std::size_t point_count) {
    if (blockIdx.x != 0 || threadIdx.x != 0) return;
    ftclFloat minx = points[0], miny = points[1], maxx = points[0], maxy = points[1];
    for (std::size_t i = 1; i < point_count; ++i) {
        const ftclFloat x = points[i * 2];
        const ftclFloat y = points[i * 2 + 1];
        if (x < minx) minx = x;
        if (y < miny) miny = y;
        if (x > maxx) maxx = x;
        if (y > maxy) maxy = y;
    }
    dst[0] = minx; dst[1] = miny; dst[2] = maxx; dst[3] = maxy;
}

__global__ void centroid_kernel(ftclFloat* dst, const ftclFloat* points, std::size_t point_count) {
    if (blockIdx.x != 0 || threadIdx.x != 0) return;
    ftclFloat sx = 0.0, sy = 0.0;
    for (std::size_t i = 0; i < point_count; ++i) {
        sx += points[i * 2];
        sy += points[i * 2 + 1];
    }
    dst[0] = sx / static_cast<ftclFloat>(point_count);
    dst[1] = sy / static_cast<ftclFloat>(point_count);
}

__global__ void transform_points_kernel(ftclFloat* dst,
                                        const ftclFloat* points,
                                        std::size_t point_count,
                                        ftclFloat m00,
                                        ftclFloat m01,
                                        ftclFloat m02,
                                        ftclFloat m10,
                                        ftclFloat m11,
                                        ftclFloat m12) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= point_count) return;
    const ftclFloat x = points[i * 2];
    const ftclFloat y = points[i * 2 + 1];
    dst[i * 2] = m00 * x + m01 * y + m02;
    dst[i * 2 + 1] = m10 * x + m11 * y + m12;
}

__global__ void batch_orientation_kernel(ftclFloat* dst, const ftclFloat* triples, std::size_t triple_count) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= triple_count) return;
    const std::size_t b = i * 6;
    dst[i] = static_cast<ftclFloat>(dorientation(triples[b], triples[b + 1], triples[b + 2], triples[b + 3], triples[b + 4], triples[b + 5]));
}

__global__ void collision_aabb_kernel(ftclFloat* dst, const ftclFloat* lhs, const ftclFloat* rhs, std::size_t box_count) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= box_count) return;
    const std::size_t b = i * 4;
    const bool hit = lhs[b] <= rhs[b + 2] + kDeviceEps && lhs[b + 2] + kDeviceEps >= rhs[b] &&
                     lhs[b + 1] <= rhs[b + 3] + kDeviceEps && lhs[b + 3] + kDeviceEps >= rhs[b + 1];
    dst[i] = hit ? 1.0 : 0.0;
}

__global__ void polygon_batch_area_kernel(ftclFloat* dst,
                                          const ftclFloat* points,
                                          const ftclFloat* offsets,
                                          std::size_t point_count,
                                          std::size_t polygon_count) {
    const std::size_t pi = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (pi >= polygon_count) return;
    const std::size_t begin = static_cast<std::size_t>(offsets[pi]);
    const std::size_t end = static_cast<std::size_t>(offsets[pi + 1]);
    if (end <= begin || end > point_count || end - begin < 3) {
        dst[pi] = 0.0;
        return;
    }
    ftclFloat twice = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
        const std::size_t j = (i + 1 == end) ? begin : i + 1;
        twice += dcross(points[i * 2], points[i * 2 + 1], points[j * 2], points[j * 2 + 1]);
    }
    dst[pi] = twice < 0.0 ? -twice / 2.0 : twice / 2.0;
}

__global__ void distance_to_polyline_kernel(ftclFloat* dst,
                                            const ftclFloat* points,
                                            const ftclFloat* polyline,
                                            std::size_t point_count,
                                            std::size_t polyline_count) {
    const std::size_t pi = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (pi >= point_count) return;
    ftclFloat best = 1.0e300;
    const ftclFloat px = points[pi * 2];
    const ftclFloat py = points[pi * 2 + 1];
    for (std::size_t i = 0; i + 1 < polyline_count; ++i) {
        const ftclFloat d = dpoint_segment_distance(px, py, polyline[i * 2], polyline[i * 2 + 1], polyline[(i + 1) * 2], polyline[(i + 1) * 2 + 1]);
        if (d < best) best = d;
    }
    dst[pi] = best;
}

__global__ void spatial_grid_build_kernel(ftclFloat* dst,
                                          const ftclFloat* points,
                                          std::size_t point_count,
                                          ftclFloat ox,
                                          ftclFloat oy,
                                          ftclFloat cell_size,
                                          std::size_t columns) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= point_count) return;
    const long long cx = static_cast<long long>(floor((points[i * 2] - ox) / cell_size));
    const long long cy = static_cast<long long>(floor((points[i * 2 + 1] - oy) / cell_size));
    dst[i] = static_cast<ftclFloat>(cy * static_cast<long long>(columns) + cx);
}

inline int blocks_for(std::size_t count) {
    return static_cast<int>((count + kThreads - 1) / kThreads);
}

}  // namespace

ftcl::expected<std::size_t, std::string> batch_distance_cuda(RawUMutPtr<ftclFloat> dst,
                                                            RawUPtr<ftclFloat> points,
                                                            Point query,
                                                            std::size_t point_count) {
    auto ready = prepare_cuda_unary(dst, points, "geometry CUDA batch_distance");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || dst.len() < point_count) return ftcl::unexpected("geometry CUDA batch_distance pointer length is too small");
    if (point_count == 0) return static_cast<std::size_t>(0);
    batch_distance_kernel<<<blocks_for(point_count), kThreads>>>(dst.get(), points.get(), query.x, query.y, point_count);
    return finish_kernel("geometry batch_distance kernel", point_count);
}

ftcl::expected<std::size_t, std::string> batch_distance_matrix_cuda(RawUMutPtr<ftclFloat> dst,
                                                                   RawUPtr<ftclFloat> lhs,
                                                                   RawUPtr<ftclFloat> rhs,
                                                                   std::size_t lhs_count,
                                                                   std::size_t rhs_count) {
    auto ready = prepare_cuda_binary(dst, lhs, rhs, "geometry CUDA batch_distance_matrix");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    const std::size_t total = lhs_count * rhs_count;
    if (lhs.len() < lhs_count * 2 || rhs.len() < rhs_count * 2 || dst.len() < total) return ftcl::unexpected("geometry CUDA batch_distance_matrix pointer length is too small");
    if (total == 0) return static_cast<std::size_t>(0);
    batch_distance_matrix_kernel<<<blocks_for(total), kThreads>>>(dst.get(), lhs.get(), rhs.get(), lhs_count, rhs_count);
    return finish_kernel("geometry batch_distance_matrix kernel", total);
}

ftcl::expected<std::size_t, std::string> batch_point_in_polygon_cuda(RawUMutPtr<ftclFloat> dst,
                                                                    RawUPtr<ftclFloat> points,
                                                                    RawUPtr<ftclFloat> polygon,
                                                                    std::size_t point_count,
                                                                    std::size_t polygon_count) {
    auto ready = prepare_cuda_binary(dst, points, polygon, "geometry CUDA batch_point_in_polygon");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || polygon.len() < polygon_count * 2 || dst.len() < point_count) return ftcl::unexpected("geometry CUDA batch_point_in_polygon pointer length is too small");
    if (point_count == 0) return static_cast<std::size_t>(0);
    batch_point_in_polygon_kernel<<<blocks_for(point_count), kThreads>>>(dst.get(), points.get(), polygon.get(), point_count, polygon_count);
    return finish_kernel("geometry batch_point_in_polygon kernel", point_count);
}

ftcl::expected<std::size_t, std::string> batch_segment_intersect_cuda(RawUMutPtr<ftclFloat> dst,
                                                                     RawUPtr<ftclFloat> lhs,
                                                                     RawUPtr<ftclFloat> rhs,
                                                                     std::size_t segment_count) {
    auto ready = prepare_cuda_binary(dst, lhs, rhs, "geometry CUDA batch_segment_intersect");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (lhs.len() < segment_count * 4 || rhs.len() < segment_count * 4 || dst.len() < segment_count) return ftcl::unexpected("geometry CUDA batch_segment_intersect pointer length is too small");
    if (segment_count == 0) return static_cast<std::size_t>(0);
    batch_segment_intersect_kernel<<<blocks_for(segment_count), kThreads>>>(dst.get(), lhs.get(), rhs.get(), segment_count);
    return finish_kernel("geometry batch_segment_intersect kernel", segment_count);
}

ftcl::expected<std::size_t, std::string> batch_point_segment_distance_cuda(RawUMutPtr<ftclFloat> dst,
                                                                          RawUPtr<ftclFloat> points,
                                                                          RawUPtr<ftclFloat> segments,
                                                                          std::size_t pair_count) {
    auto ready = prepare_cuda_binary(dst, points, segments, "geometry CUDA batch_point_segment_distance");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < pair_count * 2 || segments.len() < pair_count * 4 || dst.len() < pair_count) return ftcl::unexpected("geometry CUDA batch_point_segment_distance pointer length is too small");
    if (pair_count == 0) return static_cast<std::size_t>(0);
    batch_point_segment_distance_kernel<<<blocks_for(pair_count), kThreads>>>(dst.get(), points.get(), segments.get(), pair_count);
    return finish_kernel("geometry batch_point_segment_distance kernel", pair_count);
}

ftcl::expected<std::size_t, std::string> nearest_point_cuda(RawUMutPtr<ftclFloat> dst,
                                                           RawUPtr<ftclFloat> dataset,
                                                           RawUPtr<ftclFloat> queries,
                                                           std::size_t dataset_count,
                                                           std::size_t query_count) {
    auto ready = prepare_cuda_binary(dst, dataset, queries, "geometry CUDA nearest_point");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (dataset.len() < dataset_count * 2 || queries.len() < query_count * 2 || dst.len() < query_count * 2) return ftcl::unexpected("geometry CUDA nearest_point pointer length is too small");
    if (query_count == 0) return static_cast<std::size_t>(0);
    nearest_point_kernel<<<blocks_for(query_count), kThreads>>>(dst.get(), dataset.get(), queries.get(), dataset_count, query_count);
    return finish_kernel("geometry nearest_point kernel", query_count);
}

ftcl::expected<std::size_t, std::string> k_nearest_points_cuda(RawUMutPtr<ftclFloat> dst,
                                                              RawUPtr<ftclFloat> dataset,
                                                              RawUPtr<ftclFloat> queries,
                                                              std::size_t dataset_count,
                                                              std::size_t query_count,
                                                              std::size_t k) {
    auto ready = prepare_cuda_binary(dst, dataset, queries, "geometry CUDA k_nearest");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (dataset.len() < dataset_count * 2 || queries.len() < query_count * 2 || dst.len() < query_count * k * 2) return ftcl::unexpected("geometry CUDA k_nearest pointer length is too small");
    if (query_count == 0 || k == 0) return static_cast<std::size_t>(0);
    k_nearest_points_kernel<<<blocks_for(query_count), kThreads>>>(dst.get(), dataset.get(), queries.get(), dataset_count, query_count, k);
    return finish_kernel("geometry k_nearest kernel", query_count);
}

ftcl::expected<std::size_t, std::string> range_count_circle_cuda(RawUMutPtr<ftclFloat> dst,
                                                                RawUPtr<ftclFloat> points,
                                                                RawUPtr<ftclFloat> centers,
                                                                std::size_t point_count,
                                                                std::size_t center_count,
                                                                ftclFloat radius) {
    auto ready = prepare_cuda_binary(dst, points, centers, "geometry CUDA range_count_circle");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || centers.len() < center_count * 2 || dst.len() < center_count) return ftcl::unexpected("geometry CUDA range_count_circle pointer length is too small");
    if (center_count == 0) return static_cast<std::size_t>(0);
    range_count_circle_kernel<<<blocks_for(center_count), kThreads>>>(dst.get(), points.get(), centers.get(), point_count, center_count, radius);
    return finish_kernel("geometry range_count_circle kernel", center_count);
}

ftcl::expected<std::size_t, std::string> range_count_rect_cuda(RawUMutPtr<ftclFloat> dst,
                                                              RawUPtr<ftclFloat> points,
                                                              RawUPtr<ftclFloat> rects,
                                                              std::size_t point_count,
                                                              std::size_t rect_count) {
    auto ready = prepare_cuda_binary(dst, points, rects, "geometry CUDA range_count_rect");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || rects.len() < rect_count * 4 || dst.len() < rect_count) return ftcl::unexpected("geometry CUDA range_count_rect pointer length is too small");
    if (rect_count == 0) return static_cast<std::size_t>(0);
    range_count_rect_kernel<<<blocks_for(rect_count), kThreads>>>(dst.get(), points.get(), rects.get(), point_count, rect_count);
    return finish_kernel("geometry range_count_rect kernel", rect_count);
}

ftcl::expected<std::size_t, std::string> bbox_reduce_cuda(RawUMutPtr<ftclFloat> dst, RawUPtr<ftclFloat> points, std::size_t point_count) {
    auto ready = prepare_cuda_unary(dst, points, "geometry CUDA bbox_reduce");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || dst.len() < 4) return ftcl::unexpected("geometry CUDA bbox_reduce pointer length is too small");
    if (point_count == 0) return ftcl::unexpected("geometry CUDA bbox_reduce requires at least one point");
    bbox_reduce_kernel<<<1, 1>>>(dst.get(), points.get(), point_count);
    return finish_kernel("geometry bbox_reduce kernel", 4);
}

ftcl::expected<std::size_t, std::string> centroid_cuda(RawUMutPtr<ftclFloat> dst, RawUPtr<ftclFloat> points, std::size_t point_count) {
    auto ready = prepare_cuda_unary(dst, points, "geometry CUDA centroid");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || dst.len() < 2) return ftcl::unexpected("geometry CUDA centroid pointer length is too small");
    if (point_count == 0) return ftcl::unexpected("geometry CUDA centroid requires at least one point");
    centroid_kernel<<<1, 1>>>(dst.get(), points.get(), point_count);
    return finish_kernel("geometry centroid kernel", 2);
}

ftcl::expected<std::size_t, std::string> transform_points_cuda(RawUMutPtr<ftclFloat> dst,
                                                              RawUPtr<ftclFloat> points,
                                                              std::size_t point_count,
                                                              ftclFloat m00,
                                                              ftclFloat m01,
                                                              ftclFloat m02,
                                                              ftclFloat m10,
                                                              ftclFloat m11,
                                                              ftclFloat m12) {
    auto ready = prepare_cuda_unary(dst, points, "geometry CUDA transform_points");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || dst.len() < point_count * 2) return ftcl::unexpected("geometry CUDA transform_points pointer length is too small");
    if (point_count == 0) return static_cast<std::size_t>(0);
    transform_points_kernel<<<blocks_for(point_count), kThreads>>>(dst.get(), points.get(), point_count, m00, m01, m02, m10, m11, m12);
    return finish_kernel("geometry transform_points kernel", point_count);
}

ftcl::expected<std::size_t, std::string> batch_orientation_cuda(RawUMutPtr<ftclFloat> dst, RawUPtr<ftclFloat> triples, std::size_t triple_count) {
    auto ready = prepare_cuda_unary(dst, triples, "geometry CUDA batch_orientation");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (triples.len() < triple_count * 6 || dst.len() < triple_count) return ftcl::unexpected("geometry CUDA batch_orientation pointer length is too small");
    if (triple_count == 0) return static_cast<std::size_t>(0);
    batch_orientation_kernel<<<blocks_for(triple_count), kThreads>>>(dst.get(), triples.get(), triple_count);
    return finish_kernel("geometry batch_orientation kernel", triple_count);
}

ftcl::expected<std::size_t, std::string> collision_aabb_cuda(RawUMutPtr<ftclFloat> dst, RawUPtr<ftclFloat> lhs, RawUPtr<ftclFloat> rhs, std::size_t box_count) {
    auto ready = prepare_cuda_binary(dst, lhs, rhs, "geometry CUDA collision_aabb");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (lhs.len() < box_count * 4 || rhs.len() < box_count * 4 || dst.len() < box_count) return ftcl::unexpected("geometry CUDA collision_aabb pointer length is too small");
    if (box_count == 0) return static_cast<std::size_t>(0);
    collision_aabb_kernel<<<blocks_for(box_count), kThreads>>>(dst.get(), lhs.get(), rhs.get(), box_count);
    return finish_kernel("geometry collision_aabb kernel", box_count);
}

ftcl::expected<std::size_t, std::string> polygon_batch_area_cuda(RawUMutPtr<ftclFloat> dst,
                                                                RawUPtr<ftclFloat> points,
                                                                RawUPtr<ftclFloat> offsets,
                                                                std::size_t point_count,
                                                                std::size_t polygon_count) {
    auto ready = prepare_cuda_binary(dst, points, offsets, "geometry CUDA polygon_batch_area");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || offsets.len() < polygon_count + 1 || dst.len() < polygon_count) return ftcl::unexpected("geometry CUDA polygon_batch_area pointer length is too small");
    if (polygon_count == 0) return static_cast<std::size_t>(0);
    polygon_batch_area_kernel<<<blocks_for(polygon_count), kThreads>>>(dst.get(), points.get(), offsets.get(), point_count, polygon_count);
    return finish_kernel("geometry polygon_batch_area kernel", polygon_count);
}

ftcl::expected<std::size_t, std::string> distance_to_polyline_cuda(RawUMutPtr<ftclFloat> dst,
                                                                  RawUPtr<ftclFloat> points,
                                                                  RawUPtr<ftclFloat> polyline,
                                                                  std::size_t point_count,
                                                                  std::size_t polyline_count) {
    auto ready = prepare_cuda_binary(dst, points, polyline, "geometry CUDA distance_to_polyline");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || polyline.len() < polyline_count * 2 || dst.len() < point_count) return ftcl::unexpected("geometry CUDA distance_to_polyline pointer length is too small");
    if (point_count == 0) return static_cast<std::size_t>(0);
    distance_to_polyline_kernel<<<blocks_for(point_count), kThreads>>>(dst.get(), points.get(), polyline.get(), point_count, polyline_count);
    return finish_kernel("geometry distance_to_polyline kernel", point_count);
}

ftcl::expected<std::size_t, std::string> spatial_grid_build_cuda(RawUMutPtr<ftclFloat> dst,
                                                                RawUPtr<ftclFloat> points,
                                                                std::size_t point_count,
                                                                Point origin,
                                                                ftclFloat cell_size,
                                                                std::size_t columns) {
    auto ready = prepare_cuda_unary(dst, points, "geometry CUDA spatial_grid_build");
    if (!ready.has_value()) return ftcl::unexpected(ready.error());
    if (points.len() < point_count * 2 || dst.len() < point_count) return ftcl::unexpected("geometry CUDA spatial_grid_build pointer length is too small");
    if (point_count == 0) return static_cast<std::size_t>(0);
    spatial_grid_build_kernel<<<blocks_for(point_count), kThreads>>>(dst.get(), points.get(), point_count, origin.x, origin.y, cell_size, columns);
    return finish_kernel("geometry spatial_grid_build kernel", point_count);
}

}  // namespace ftcl::geom

#endif

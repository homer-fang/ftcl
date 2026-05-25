#include "geometry_cuda.hpp"

#ifdef FTCL_GEOMETRY_CUDA

#include <cuda_runtime.h>

#include <string>

namespace ftcl::geom {
namespace {

__global__ void batch_distance_kernel(ftclFloat* dst,
                                      const ftclFloat* points,
                                      ftclFloat qx,
                                      ftclFloat qy,
                                      std::size_t point_count) {
    const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= point_count) {
        return;
    }

    const ftclFloat dx = points[i * 2] - qx;
    const ftclFloat dy = points[i * 2 + 1] - qy;
    dst[i] = sqrt(dx * dx + dy * dy);
}

inline std::string cuda_error(const char* op, cudaError_t err) {
    return std::string(op) + " failed: " + cudaGetErrorString(err);
}

}  // namespace

ftcl::expected<std::size_t, std::string> batch_distance_cuda(RawUMutPtr<ftclFloat> dst,
                                                            RawUPtr<ftclFloat> points,
                                                            Point query,
                                                            std::size_t point_count) {
    if (!dst.device().is_cuda() || !points.device().is_cuda()) {
        return ftcl::unexpected("geometry CUDA batch_distance requires CUDA pointers");
    }
    if (dst.device() != points.device()) {
        return ftcl::unexpected("geometry CUDA batch_distance requires source and destination on the same CUDA device");
    }
    if (points.len() < point_count * 2 || dst.len() < point_count) {
        return ftcl::unexpected("geometry CUDA batch_distance pointer length is too small");
    }
    if (point_count == 0) {
        return static_cast<std::size_t>(0);
    }

    const auto set_device = cudaSetDevice(dst.device().index());
    if (set_device != cudaSuccess) {
        return ftcl::unexpected(cuda_error("cudaSetDevice", set_device));
    }

    constexpr int threads = 256;
    const int blocks = static_cast<int>((point_count + threads - 1) / threads);
    batch_distance_kernel<<<blocks, threads>>>(dst.get(), points.get(), query.x, query.y, point_count);

    const auto launch = cudaGetLastError();
    if (launch != cudaSuccess) {
        return ftcl::unexpected(cuda_error("geometry batch_distance kernel launch", launch));
    }

    const auto sync = cudaDeviceSynchronize();
    if (sync != cudaSuccess) {
        return ftcl::unexpected(cuda_error("geometry batch_distance kernel sync", sync));
    }

    return point_count;
}

}  // namespace ftcl::geom

#endif

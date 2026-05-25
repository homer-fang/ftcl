#pragma once

#include "expected_compat.hpp"
#include "geometry.hpp"
#include "type.hpp"
#include "uvec.hpp"

#include <cstddef>
#include <string>

namespace ftcl::geom {

#ifdef FTCL_GEOMETRY_CUDA
ftcl::expected<std::size_t, std::string> batch_distance_cuda(RawUMutPtr<ftclFloat> dst,
                                                            RawUPtr<ftclFloat> points,
                                                            Point query,
                                                            std::size_t point_count);
#endif

}  // namespace ftcl::geom

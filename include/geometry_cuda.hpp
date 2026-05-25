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

ftcl::expected<std::size_t, std::string> batch_distance_matrix_cuda(RawUMutPtr<ftclFloat> dst,
                                                                   RawUPtr<ftclFloat> lhs,
                                                                   RawUPtr<ftclFloat> rhs,
                                                                   std::size_t lhs_count,
                                                                   std::size_t rhs_count);
ftcl::expected<std::size_t, std::string> batch_point_in_polygon_cuda(RawUMutPtr<ftclFloat> dst,
                                                                    RawUPtr<ftclFloat> points,
                                                                    RawUPtr<ftclFloat> polygon,
                                                                    std::size_t point_count,
                                                                    std::size_t polygon_count);
ftcl::expected<std::size_t, std::string> batch_segment_intersect_cuda(RawUMutPtr<ftclFloat> dst,
                                                                     RawUPtr<ftclFloat> lhs,
                                                                     RawUPtr<ftclFloat> rhs,
                                                                     std::size_t segment_count);
ftcl::expected<std::size_t, std::string> batch_point_segment_distance_cuda(RawUMutPtr<ftclFloat> dst,
                                                                          RawUPtr<ftclFloat> points,
                                                                          RawUPtr<ftclFloat> segments,
                                                                          std::size_t pair_count);
ftcl::expected<std::size_t, std::string> nearest_point_cuda(RawUMutPtr<ftclFloat> dst,
                                                           RawUPtr<ftclFloat> dataset,
                                                           RawUPtr<ftclFloat> queries,
                                                           std::size_t dataset_count,
                                                           std::size_t query_count);
ftcl::expected<std::size_t, std::string> k_nearest_points_cuda(RawUMutPtr<ftclFloat> dst,
                                                              RawUPtr<ftclFloat> dataset,
                                                              RawUPtr<ftclFloat> queries,
                                                              std::size_t dataset_count,
                                                              std::size_t query_count,
                                                              std::size_t k);
ftcl::expected<std::size_t, std::string> range_count_circle_cuda(RawUMutPtr<ftclFloat> dst,
                                                                RawUPtr<ftclFloat> points,
                                                                RawUPtr<ftclFloat> centers,
                                                                std::size_t point_count,
                                                                std::size_t center_count,
                                                                ftclFloat radius);
ftcl::expected<std::size_t, std::string> range_count_rect_cuda(RawUMutPtr<ftclFloat> dst,
                                                              RawUPtr<ftclFloat> points,
                                                              RawUPtr<ftclFloat> rects,
                                                              std::size_t point_count,
                                                              std::size_t rect_count);
ftcl::expected<std::size_t, std::string> bbox_reduce_cuda(RawUMutPtr<ftclFloat> dst,
                                                         RawUPtr<ftclFloat> points,
                                                         std::size_t point_count);
ftcl::expected<std::size_t, std::string> centroid_cuda(RawUMutPtr<ftclFloat> dst,
                                                      RawUPtr<ftclFloat> points,
                                                      std::size_t point_count);
ftcl::expected<std::size_t, std::string> transform_points_cuda(RawUMutPtr<ftclFloat> dst,
                                                              RawUPtr<ftclFloat> points,
                                                              std::size_t point_count,
                                                              ftclFloat m00,
                                                              ftclFloat m01,
                                                              ftclFloat m02,
                                                              ftclFloat m10,
                                                              ftclFloat m11,
                                                              ftclFloat m12);
ftcl::expected<std::size_t, std::string> batch_orientation_cuda(RawUMutPtr<ftclFloat> dst,
                                                               RawUPtr<ftclFloat> triples,
                                                               std::size_t triple_count);
ftcl::expected<std::size_t, std::string> collision_aabb_cuda(RawUMutPtr<ftclFloat> dst,
                                                            RawUPtr<ftclFloat> lhs,
                                                            RawUPtr<ftclFloat> rhs,
                                                            std::size_t box_count);
ftcl::expected<std::size_t, std::string> polygon_batch_area_cuda(RawUMutPtr<ftclFloat> dst,
                                                                RawUPtr<ftclFloat> points,
                                                                RawUPtr<ftclFloat> offsets,
                                                                std::size_t point_count,
                                                                std::size_t polygon_count);
ftcl::expected<std::size_t, std::string> distance_to_polyline_cuda(RawUMutPtr<ftclFloat> dst,
                                                                  RawUPtr<ftclFloat> points,
                                                                  RawUPtr<ftclFloat> polyline,
                                                                  std::size_t point_count,
                                                                  std::size_t polyline_count);
ftcl::expected<std::size_t, std::string> spatial_grid_build_cuda(RawUMutPtr<ftclFloat> dst,
                                                                RawUPtr<ftclFloat> points,
                                                                std::size_t point_count,
                                                                Point origin,
                                                                ftclFloat cell_size,
                                                                std::size_t columns);
#endif

}  // namespace ftcl::geom

#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Single-linkage connected components over a 3D point cloud: any two points
// closer than the cluster distance belong to the same blob.
//
// Ported from the 3D Blob Tracker TouchDesigner POP (src/ClusterKernels.*),
// where the same pipeline runs as OpenGL 4.3 compute shaders. The stages are
// kept one-for-one — bbox + cell histogram, prefix sum, scatter into cell
// order, 27-cell union-find, label, accumulate, build — because each one's
// output is what the next one's correctness argument rests on, and because the
// k-NN spacing estimate reads the very grid the clustering built.
//
// This is the CPU implementation. The pipeline also exists as QRhi compute in
// GpuClustering.hpp — a transliteration of the same twelve passes, and the path
// to use for a cloud that already lives on the GPU. This one has two jobs the
// GPU one cannot do: it clusters a plain CPU point array (no render thread, no
// readback), and it is the oracle the GPU pipeline is tested against, which is
// what makes it worth keeping exact where the shaders have to approximate.
//
// Two differences from the GPU original, both things the shaders could not do:
//
//  - Centroids accumulate in double rather than as integers quantised against
//    the frame bounding box. The quantisation existed because GPU atomics have
//    no float add; int32 sums then overflow at millimetre-scale coordinates,
//    which is why the original re-quantised every frame.
//  - Blobs come out in the order their first point appears in the input,
//    instead of in whatever order atomicAdd handed out cluster ids. A given
//    cloud therefore always produces the same blobs in the same order, and the
//    Max blobs cap keeps the first ones rather than an arbitrary subset.
//
// Non-finite positions are skipped in every stage. Depth sensors emit NaN and
// Inf for invalid pixels; on the GPU a NaN operand made the atomic min/max
// compare-and-swap loops spin forever, and here it would poison a bounding box
// and every comparison against it.

#include <cstdint>
#include <vector>

namespace Threedim::Blobs
{

// One connected component that passed the minimum-point filter.
struct Detection
{
  float cx{}, cy{}, cz{};
  float bmnx{}, bmny{}, bmnz{};
  float bmxx{}, bmxy{}, bmxz{};
  int32_t point_count{};
};

// Blobs returned by one detect() call. max_blobs is clamped to this.
inline constexpr int max_blobs_limit = 4096;

// Distinct components labelled in one frame. Components past this are dropped
// and reported through cluster_overflow. Blob tracking deals in tens to
// hundreds of blobs, so this is far above any real use.
inline constexpr int max_clusters = 65536;

// Spatial hash table bounds, and the k-NN sampling constants. Same values as
// the GPU pipeline: the table is a power of two so bucketOf can mask.
inline constexpr uint32_t min_table_size = 1u << 10;
inline constexpr uint32_t max_table_size = 1u << 22;
inline constexpr int knn_samples = 256;
inline constexpr int knn_max_k = 8;

// Candidate cap per k-NN sample. Only reached while the cluster distance is far
// too large for the cloud, which lasts a frame or two before auto-scale settles.
inline constexpr int knn_max_candidates = 2048;

struct ClusterParams
{
  float cluster_dist = 0.1f;
  int min_points = 100;
  int max_blobs = 100;

  // k for the nearest-neighbour spacing estimate that drives auto-scale. The
  // result lands in ClusterResult::knn_spacing. 0 skips the estimate.
  int knn_k = 0;
};

struct ClusterResult
{
  int num_blobs = 0;
  int num_valid_points = 0; // input points that were finite
  int num_clusters = 0;     // components found, before the min_points filter
  bool cluster_overflow = false;
  float knn_spacing = 0.f;

  // Bounding box of the finite points. The GPU pipeline needs it anyway (it is
  // the frame the fixed-point centroid accumulation is quantised against), so
  // it costs nothing to report — and it is the one value that shows whether the
  // per-frame clear actually ran, since an uncleared accumulator reads as zero
  // rather than as the ±FLT_MAX the reduction starts from.
  float bounds_min[3]{};
  float bounds_max[3]{};
};

// Holds its scratch across calls, so a steady-state frame allocates nothing.
class Clusterer
{
public:
  // positions is point_count tightly packed xyz triples. Returns the number of
  // blobs written to `blobs`, which is resized to exactly that.
  int detect(
      const float* positions, int64_t point_count, const ClusterParams& params,
      std::vector<Detection>& blobs, ClusterResult& result);

  // The valid points in cell order, as the grid stages left them. Exposed for
  // tests; the tracker does not need it.
  const std::vector<float>& sorted_positions() const noexcept { return m_sorted; }

private:
  void build_grid(int32_t n, float inv_cell_size, uint32_t table_size);
  void union_neighbours(int32_t n, float inv_cell_size, uint32_t table_size, float d2);
  int32_t label(int32_t n, bool& overflow);
  void accumulate(int32_t n, int32_t clusters);
  float estimate_spacing(int32_t n, float inv_cell_size, uint32_t table_size, int k);

  int32_t find(int32_t x) noexcept;

  std::vector<float> m_sorted;        // 3 floats per valid point, cell order
  std::vector<int32_t> m_origin;      // where each sorted point sat in the input
  std::vector<float> m_scratch;       // valid points before the scatter
  std::vector<uint32_t> m_cell_start; // table_size + 1 entries
  std::vector<uint32_t> m_cursor;
  std::vector<int32_t> m_parent;
  std::vector<int32_t> m_cluster;

  std::vector<double> m_sum;   // 3 per cluster
  std::vector<float> m_bounds; // 6 per cluster: min xyz then max xyz
  std::vector<int32_t> m_counts;
  std::vector<int32_t> m_first; // earliest input point of each cluster
  std::vector<int32_t> m_emit;  // qualifying clusters, in input order

  std::vector<float> m_knn;
};

}

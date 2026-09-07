#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// The blob clustering pipeline as QRhi compute — the same twelve passes the
// TouchDesigner original ran as OpenGL 4.3 compute shaders, ported to the
// GLSL 450 / SPIR-V dialect score compiles through score::gfx::makeCompute.
//
//   reset -> bbox+histogram -> scan(3) -> scatter -> union-find merge
//         -> label -> propagate -> accumulate -> build blobs -> k-NN spacing
//
// The stage boundaries are load-bearing, not stylistic: each one's whole output
// is the next one's input, so every boundary needs a compute barrier
// (score::gfx::insertComputeBarrier, which is why this cannot collapse into
// fewer dispatches).
//
// Two things differ from the original, both because QRhi is not raw GL:
//
//  - There is no glClearBufferSubData, so the per-frame clears (histogram,
//    accumulator, meta) are their own compute pass rather than driver-side
//    buffer fills.
//  - Positions are read through a stride/offset rather than from a tightly
//    packed P buffer, so any geometry's position attribute can be clustered in
//    place — no repacking pass in front of it.
//
// Everything lands in ONE compact results buffer (meta, k-NN samples, blobs)
// so a frame costs a single readback, which is what keeps the pipeline down to
// one host/device sync exactly as the original did. Threedim::Blobs::decode
// turns that buffer back into Detections; the percentile over the k-NN samples
// is computed host-side there, as upstream.
//
// The centroid accumulation keeps the original's per-frame fixed-point frame
// (see the comment on `scale` in the scan-sums pass): GLSL 450 has no atomic
// float add, so integer sums quantised against the frame bounding box are still
// the only portable way to reduce a centroid on the GPU. The CPU Clusterer in
// Clustering.hpp sums in double and is the reference the GPU path is tested
// against.

#include <Threedim/BlobTracker/Clustering.hpp>

#include <cstdint>
#include <vector>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiComputePipeline;
class QRhiResourceUpdateBatch;
class QRhiShaderResourceBindings;

namespace score::gfx
{
struct RenderState;
}

namespace Threedim::Blobs
{

// Layout of the results buffer, in 4-byte words. meta mirrors the original's
// meta[] indices exactly (0-5 bbox, 6-8 origin, 9-11 scale, 12 valid points,
// 13 cluster count, 14 blob count, 15 overflow).
inline constexpr int gpu_meta_words = 16;
inline constexpr int gpu_blob_words = 10; // 3 centroid, 6 box, 1 count

// Bytes the results buffer occupies for a given blob cap.
[[nodiscard]] constexpr int64_t gpu_results_bytes(int max_blobs) noexcept
{
  return (int64_t)(gpu_meta_words + knn_samples + max_blobs * gpu_blob_words)
         * sizeof(uint32_t);
}

// Where the point positions live: any buffer with a float3 at a fixed stride,
// so a geometry's interleaved vertex buffer can be clustered in place.
struct GpuPointSource
{
  QRhiBuffer* buffer{};
  int64_t count{};        // number of points
  int32_t stride_bytes{}; // distance between consecutive positions
  int32_t offset_bytes{}; // where the first position starts

  [[nodiscard]] bool valid() const noexcept
  {
    return buffer && count > 0 && stride_bytes >= 12 && (stride_bytes % 4) == 0
           && (offset_bytes % 4) == 0;
  }
};

class GpuClusterPipeline
{
public:
  ~GpuClusterPipeline();

  // Builds the pipelines and the scratch buffers. `max_points` and `max_blobs`
  // size the allocations; both grow on demand in update().
  [[nodiscard]] bool init(
      const score::gfx::RenderState& state, QRhi& rhi, int64_t max_points,
      int max_blobs);

  void release() noexcept;

  [[nodiscard]] bool initialized() const noexcept { return m_ready; }

  // Point the pipeline at this frame's cloud and settings. Reallocates when the
  // cloud outgrew the current buffers. Returns false if it cannot run this
  // frame (invalid source, allocation failure).
  [[nodiscard]] bool update(
      const score::gfx::RenderState& state, QRhi& rhi, const GpuPointSource& src,
      const ClusterParams& params);

  // Runs the twelve passes. Consumes `res` and hands back a fresh batch, in the
  // same contract as Threedim's other compute strategies.
  void runCompute(QRhi& rhi, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res);

  // The buffer to read back: meta, k-NN samples and blobs, tightly packed.
  [[nodiscard]] QRhiBuffer* results() const noexcept { return m_results; }
  [[nodiscard]] int64_t results_bytes() const noexcept
  {
    return gpu_results_bytes(m_max_blobs);
  }
  [[nodiscard]] int max_blobs() const noexcept { return m_max_blobs; }

private:
  enum Pass
  {
    Reset,
    BboxCount,
    ScanBlocks,
    ScanSums,
    ScanAdd,
    Scatter,
    Merge,
    Label,
    Propagate,
    Accumulate,
    BuildBlobs,
    Knn,
    PassCount
  };

  struct Kernel
  {
    QRhiComputePipeline* pipeline{};
    QRhiShaderResourceBindings* srb{};
  };

  [[nodiscard]] bool
  buildKernel(const score::gfx::RenderState&, QRhi&, Pass, const char* body);
  [[nodiscard]] bool allocate(QRhi& rhi, int64_t points, int max_blobs);
  // False when a compute pipeline could not be built. A shader that a driver
  // refuses to link is a bug, not a capability gap, and must stop the pipeline
  // rather than let every later pass run against buffers nothing initialised.
  [[nodiscard]] bool bindAll(QRhi& rhi);
  // deferred: hand the buffers to QRhi's own deletion queue (a mid-session
  // realloc, with a frame possibly still referencing them) rather than deleting
  // them outright (teardown, where the caller guarantees no frame is live).
  void destroyBuffers(bool deferred) noexcept;

  Kernel m_kernels[PassCount];
  QRhiBuffer* m_ubo{};

  QRhiBuffer* m_sorted{};
  QRhiBuffer* m_count{};
  QRhiBuffer* m_start{};
  QRhiBuffer* m_blocks{};
  QRhiBuffer* m_parent{};
  QRhiBuffer* m_cluster{};
  QRhiBuffer* m_accum{};
  QRhiBuffer* m_results{};

  GpuPointSource m_src;
  ClusterParams m_params;

  int64_t m_capacity{}; // points the scratch buffers are sized for
  int m_max_blobs{};    // blobs the results buffer is sized for
  uint32_t m_table_size{};
  bool m_ready{};
  bool m_pipelines_created{};
  bool m_bindings_dirty{true};
};

// Turns a readback of the results buffer into blobs. `bytes` may be short (a
// dropped readback); the decode then reports nothing rather than reading past
// the data. Returns the number of blobs written.
int decode_gpu_results(
    const void* data, int64_t bytes, int max_blobs, std::vector<Detection>& blobs,
    ClusterResult& result);

}

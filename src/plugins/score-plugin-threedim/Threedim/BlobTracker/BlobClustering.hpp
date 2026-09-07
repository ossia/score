#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Clusters a point cloud into blobs on the GPU, in QRhi compute.
//
// The clustering half of the 3D Blob Tracker port: the object is a thin wrapper
// over Threedim::Blobs::GpuClusterPipeline, which holds the twelve compute
// passes. It reads the position attribute of whatever geometry is connected —
// in place, through its stride — and publishes one compact results buffer
// (counts, k-NN samples, blobs).
//
// Wire the Blobs outlet into 3D Blob Tracker's Blobs inlet: that inlet is a CPU
// buffer, so the engine reads this buffer back and the tracker sees the frame's
// detections. Everything downstream of the readback is CPU work on a handful of
// blobs, which is where it belongs.

#include <Threedim/BlobTracker/GpuClustering.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace score::gfx
{
class RenderList;
struct Edge;
}
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;

namespace Threedim
{

class BlobClustering
{
public:
  halp_meta(name, "3D Blob Clustering")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "blob_clustering_3d")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/blob-tracker-3d.html")
  halp_meta(uuid, "06058d8f-c6d0-41e1-a08d-1cb93164389b")
  halp_meta(author, "Lou Evoy, Jean-Michaël Celerier")
  halp_meta(description, "Cluster a 3D point cloud into blobs, on the GPU")

  struct ins
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;

    halp::hslider_f32<"Cluster distance", halp::range{0.0001, 10., 0.1}> cluster_dist;
    halp::spinbox_i32<"Min points per blob", halp::irange{1, 100000, 100}> min_points;
    halp::spinbox_i32<"Max blobs", halp::irange{1, Blobs::max_blobs_limit, 100}>
        max_blobs;

    // 0 turns the nearest-neighbour spacing measurement off. The tracker's Auto
    // scale needs it; nothing else does.
    halp::spinbox_i32<"Spacing neighbours", halp::irange{0, Blobs::knn_max_k, 6}> knn_k;
  } inputs;

  struct outs
  {
    halp::gpu_buffer_output<"Blobs"> blobs;
  } outputs;

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& renderer);
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge);

  void operator()();

private:
  // Where this frame's positions live, resolved from the geometry's position
  // attribute. Invalid when there is no usable geometry.
  [[nodiscard]] Blobs::GpuPointSource resolveSource() const noexcept;
  [[nodiscard]] Blobs::ClusterParams params() const noexcept;
  void publish() noexcept;

  Blobs::GpuClusterPipeline m_pipeline;
  bool m_runnable{};
};

}

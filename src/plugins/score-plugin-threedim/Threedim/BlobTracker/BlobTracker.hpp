#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Finds blobs in a 3D point cloud and tracks them with stable ids across
// frames. A port of the 3D Blob Tracker TouchDesigner POP by Lou Evoy
// (github.com/louevoy — "3D Blob Tracker GPU"); see BlobTracker/Clustering.hpp
// and BlobTracker/Tracker.hpp for what the port kept and what it changed.
//
// The object is a thin adapter: it maps controls onto Threedim::Blobs::Tracker,
// which holds every piece of the algorithm and is what the tests drive.

#include <Threedim/BlobTracker/GpuClustering.hpp>
#include <Threedim/BlobTracker/Tracker.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#include <string>
#include <vector>

namespace Threedim
{

// One tracked blob, as a flat list value: id, centroid xyz, box size xyz,
// velocity xyz, point count.
struct BlobValue
{
  int id{};
  float x{}, y{}, z{};
  float size_x{}, size_y{}, size_z{};
  float velocity_x{}, velocity_y{}, velocity_z{};
  int points{};
};

class BlobTracker
{
public:
  halp_meta(name, "3D Blob Tracker")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "blob_tracker_3d")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/blob-tracker-3d.html")
  halp_meta(uuid, "37715edc-80e4-41d1-82ce-662c9b440ca6")
  halp_meta(author, "Lou Evoy, Jean-Michaël Celerier")
  halp_meta(
      description, "Cluster a 3D point cloud into blobs and track them with stable ids")

  struct ins
  {
    // xyz triples, as produced by a depth sensor or by any object that emits a
    // flat point array. Clustered on the CPU. Non-finite points are skipped.
    halp::val_port<"Points", std::vector<float>> points;

    // Detections from 3D Blob Clustering, which does the clustering in QRhi
    // compute. A CPU-buffer inlet, so the engine reads that results buffer back
    // for us. It takes precedence over Points whenever it carries a frame.
    halp::cpu_buffer_input<"Blobs"> blobs_in;

    // With Auto scale on, Cluster distance and Max match distance are derived
    // from the measured point spacing and their values only matter on the very
    // first frame.
    struct : halp::toggle<"Auto scale">
    {
      struct range
      {
        bool init = true;
      };
    } auto_scale;
    halp::hslider_f32<"Scale factor", halp::range{0.01, 2., 0.25}> scale_factor;
    halp::hslider_f32<"Cluster distance", halp::range{0.0001, 10., 0.1}> cluster_dist;
    halp::spinbox_i32<"Min points per blob", halp::irange{1, 100000, 100}> min_points;
    halp::spinbox_i32<"Max blobs", halp::irange{1, Blobs::max_blobs_limit, 100}>
        max_blobs;

    halp::hslider_f32<"Max match distance", halp::range{0.0001, 10., 0.5}> match_dist;
    halp::spinbox_i32<"Max track age", halp::irange{0, 600, 5}> max_age;
    halp::spinbox_i32<"Min hit streak", halp::irange{1, 100, 3}> min_hits;

    struct : halp::toggle<"Point count matching">
    {
      struct range
      {
        bool init = true;
      };
    } use_point_count;
    halp::hslider_f32<"Point count weight", halp::range{0., 5., 0.5}> point_count_weight;

    halp::spinbox_i32<"Merge memory (frames)", halp::irange{0, 600, 30}> merge_memory;
    halp::hslider_f32<"Smoothing amount", halp::range{0., 1., 0.}> smoothing;
    halp::hslider_f32<"Velocity threshold", halp::range{0., 1., 0.001}>
        velocity_threshold;

    struct : halp::impulse_button<"Reset ids">
    {
      void update(BlobTracker& self) { self.m_reset_requested = true; }
    } reset;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::dynamic_geometry mesh;
      float transform[16]{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                          0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;

    halp::val_port<"Blobs", std::vector<BlobValue>> blobs;
    halp::val_port<"Count", int> count;
    halp::val_port<"Ids", std::vector<int>> ids;
    halp::val_port<"Centroids", std::vector<float>> centroids;

    // Clusters that passed the Min points filter this frame, before tracking
    // decided which of them to report.
    halp::val_port<"Detected", int> detected;
    // True while this frame's detections came from the GPU clustering inlet.
    halp::val_port<"On GPU", bool> on_gpu;
    halp::val_port<"Point spacing", float> spacing;
    halp::val_port<"Cluster distance", float> cluster_distance;
  } outputs;

  // The blobs are reported in the point cloud's own coordinate space, so the
  // geometry's transform is the identity — but it still has to be published
  // once for the renderer to pick it up.
  BlobTracker() { outputs.geometry.dirty_transform = true; }

  void operator()();

  Blobs::Tracker& tracker() noexcept { return m_tracker; }

private:
  void write_geometry();

  Blobs::Tracker m_tracker;

  // Decoded from the Blobs inlet when the GPU path is in use.
  std::vector<Blobs::Detection> m_gpu_detections;
  std::vector<Blobs::Blob> m_gpu_blobs;

  // The geometry output points into these; the engine copies out of them
  // during the same tick.
  std::vector<float> m_vertices;
  std::vector<uint32_t> m_indices;

  bool m_reset_requested = false;
};

}

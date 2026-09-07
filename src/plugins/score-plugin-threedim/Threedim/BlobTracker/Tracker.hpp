#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Frame-level orchestration: cluster the cloud, associate the detections with
// the live tracks, hand ids back across merges and splits, and lay the result
// out as a wireframe.
//
// Ported from the 3D Blob Tracker TouchDesigner POP (src/BlobTrackerPOP.cpp).
// Everything the operator did that was not TouchDesigner plumbing lives here,
// so the score object stays a thin adapter and the tests drive the real thing.

#include <Threedim/BlobTracker/Tracking.hpp>

#include <utility>

namespace Threedim::Blobs
{

struct TrackerSettings
{
  // With auto_scale on, cluster_dist and match_dist are driven from the
  // measured point spacing and their values are only used for the first frame.
  bool auto_scale = true;
  float scale_factor = 0.25f;
  float cluster_dist = 0.1f;
  float match_dist = 0.5f;

  int min_points = 100;
  int max_blobs = 100;
  int max_age = 5;
  int min_hits = 3;

  bool use_point_count = true;
  float point_count_weight = 0.5f;

  int merge_memory_frames = 30;
  float smoothing = 0.f;
  float velocity_threshold = 0.001f;
};

// k for the nearest-neighbour spacing estimate that drives auto-scale.
inline constexpr int auto_scale_k = 6;

// Auto-scale reacts over a few frames rather than instantly, so one noisy
// estimate cannot make every blob merge or shatter for a frame.
inline constexpr float spacing_smoothing = 0.25f;

// Cluster distance as a multiple of measured point spacing. The Scale Factor
// control defaults to 0.25, giving a cluster distance of twice the spacing.
inline constexpr float spacing_to_cluster_dist = 8.f;

// Match distance derived from cluster distance when auto-scale is on.
inline constexpr float cluster_dist_to_match_dist = 5.f;

class Tracker
{
public:
  // Clears every track, merge memory and measurement, and restarts ids at zero.
  void reset() noexcept;

  // One frame: cluster `positions` (point_count tightly packed xyz triples),
  // then track. A null or empty cloud is a normal state for a live sensor, not
  // an error — the tracks still age, so stale blobs expire instead of freezing.
  void process(const float* positions, int64_t point_count, const TrackerSettings& s);

  // One frame of tracking over detections produced elsewhere.
  void track(const std::vector<Blob>& detections, const TrackerSettings& s);

  // One frame of tracking over detections the GPU pipeline produced. `res`
  // carries the spacing measurement, so Auto scale keeps working across the
  // readback — but only over the match distance: on the GPU path the cluster
  // distance is a control on the clustering node, so closing that half of the
  // loop means wiring this tracker's Cluster distance outlet back into it.
  void track_gpu(
      const std::vector<Blob>& detections, const ClusterResult& res,
      const TrackerSettings& s);

  // The blobs reported this frame: smoothed when Smoothing is above zero.
  const std::vector<Blob>& blobs() const noexcept
  {
    return m_smoothing_active ? m_smoothed : m_tracked;
  }

  const ClusterResult& cluster_result() const noexcept { return m_cluster_result; }

  // Nearest-neighbour spacing measured on the cloud, smoothed across frames.
  float point_spacing() const noexcept { return m_point_spacing; }
  // The distances actually used this frame, after auto-scale.
  float cluster_distance() const noexcept { return m_cluster_dist; }
  float match_distance() const noexcept { return m_match_dist; }

  int frame_count() const noexcept { return m_frame_count; }
  int live_track_count() const noexcept { return (int)m_trackers.size(); }
  int merge_memory_count() const noexcept { return (int)m_merge_memories.size(); }
  int next_id() const noexcept { return m_next_id; }

private:
  void apply_scale(const TrackerSettings& s) noexcept;
  void track_detections(const std::vector<Blob>& detections, const TrackerSettings& s);
  void
  absorb_unmatched_tracks(const std::vector<Blob>& detections, const TrackerSettings& s);
  void
  recover_split_tracks(const std::vector<Blob>& detections, const TrackerSettings& s);
  void apply_smoothing(float smoothing);

  Clusterer m_clusterer;
  ClusterResult m_cluster_result;
  std::vector<Detection> m_detection_scratch;

  std::vector<KalmanTracker3D> m_trackers;
  std::vector<MergeMemory> m_merge_memories;
  std::vector<Blob> m_tracked;
  std::vector<Blob> m_smoothed;

  // Reused across frames so a steady-state frame allocates nothing.
  std::vector<Blob> m_detections;
  std::vector<Blob> m_predictions;
  std::vector<float> m_cost;
  std::vector<char> m_det_matched;
  std::vector<char> m_trk_matched;
  std::vector<std::pair<int, int>> m_matches;
  HungarianSolver m_solver;

  // Auto-scale carries the spacing measured on the previous frame, which is
  // what kept the GPU pipeline down to a single sync.
  float m_point_spacing = 0.f;
  float m_cluster_dist = 0.f;
  float m_match_dist = 0.f;

  int m_frame_count = 0;
  int m_next_id = 0;
  bool m_smoothing_active = false;
};

// Per tracked blob: the centroid, the 8 bounding-box corners, and a 3-point
// velocity arrow; 12 box edges plus the arrow's shaft and two barbs.
inline constexpr int points_per_blob = 12;
inline constexpr int lines_per_blob = 15;

// Lays the blobs out as a line list. A vertex is 4 floats — xyz then the track
// id, so a downstream shader can colour or filter by track. Indices are pairs
// of vertices, one pair per line.
void build_wireframe(
    const std::vector<Blob>& blobs, float velocity_threshold,
    std::vector<float>& vertices, std::vector<uint32_t>& indices);

}

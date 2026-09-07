#include "Tracker.hpp"

#include <cmath>

#include <unordered_map>

namespace Threedim::Blobs
{

void Tracker::reset() noexcept
{
  m_trackers.clear();
  m_merge_memories.clear();
  m_tracked.clear();
  m_smoothed.clear();
  m_detections.clear();
  m_predictions.clear();
  m_matches.clear();
  m_cluster_result = ClusterResult{};
  m_point_spacing = 0.f;
  m_cluster_dist = 0.f;
  m_match_dist = 0.f;
  m_frame_count = 0;
  m_next_id = 0;
  m_smoothing_active = false;
}

void Tracker::apply_scale(const TrackerSettings& s) noexcept
{
  m_cluster_dist = s.cluster_dist;
  m_match_dist = s.match_dist;

  // The very first frame has no measurement yet and falls back to the controls.
  if(s.auto_scale && m_point_spacing > 0.f)
  {
    m_cluster_dist = m_point_spacing * s.scale_factor * spacing_to_cluster_dist;
    m_match_dist = m_cluster_dist * cluster_dist_to_match_dist;
  }
}

void Tracker::process(
    const float* positions, int64_t point_count, const TrackerSettings& s)
{
  apply_scale(s);

  if(!positions || point_count <= 0)
  {
    m_cluster_result = ClusterResult{};
    m_detections.clear();
    track_detections(m_detections, s);
    apply_smoothing(s.smoothing);
    return;
  }

  ClusterParams params;
  params.cluster_dist = m_cluster_dist;
  params.min_points = s.min_points;
  params.max_blobs = s.max_blobs;
  params.knn_k = s.auto_scale ? auto_scale_k : 0;

  const int detected = m_clusterer.detect(
      positions, point_count, params, m_detection_scratch, m_cluster_result);

  if(m_cluster_result.knn_spacing > 0.f)
  {
    m_point_spacing = (m_point_spacing > 0.f)
                          ? m_point_spacing
                                + (m_cluster_result.knn_spacing - m_point_spacing)
                                      * spacing_smoothing
                          : m_cluster_result.knn_spacing;
  }

  m_detections.resize(detected);
  for(int i = 0; i < detected; i++)
    m_detections[i] = Blob::from_detection(m_detection_scratch[i]);

  track_detections(m_detections, s);
  apply_smoothing(s.smoothing);
}

void Tracker::track(const std::vector<Blob>& detections, const TrackerSettings& s)
{
  apply_scale(s);
  track_detections(detections, s);
  apply_smoothing(s.smoothing);
}

void Tracker::track_gpu(
    const std::vector<Blob>& detections, const ClusterResult& res,
    const TrackerSettings& s)
{
  // Scale first, then absorb the measurement: this frame runs on the previous
  // frame's spacing, which is what let the original keep to a single sync.
  apply_scale(s);

  m_cluster_result = res;
  if(res.knn_spacing > 0.f)
  {
    m_point_spacing
        = (m_point_spacing > 0.f)
              ? m_point_spacing + (res.knn_spacing - m_point_spacing) * spacing_smoothing
              : res.knn_spacing;
  }

  track_detections(detections, s);
  apply_smoothing(s.smoothing);
}

void Tracker::track_detections(
    const std::vector<Blob>& detections, const TrackerSettings& s)
{
  m_frame_count++;

  // Predict every track forward, dropping any whose filter has diverged.
  m_predictions.clear();
  for(std::size_t t = 0; t < m_trackers.size();)
  {
    const Blob pred = m_trackers[t].predict();
    if(std::isnan(pred.cx) || std::isnan(pred.cy) || std::isnan(pred.cz))
    {
      m_trackers.erase(m_trackers.begin() + t);
      continue;
    }
    m_predictions.push_back(pred);
    t++;
  }

  const int num_dets = (int)detections.size();
  const int num_trks = (int)m_trackers.size();

  m_det_matched.assign(std::max(num_dets, 0), 0);
  m_trk_matched.assign(std::max(num_trks, 0), 0);
  m_matches.clear();

  if(num_dets > 0 && num_trks > 0)
  {
    m_cost.resize((std::size_t)num_dets * num_trks);
    for(int d = 0; d < num_dets; d++)
      for(int t = 0; t < num_trks; t++)
        m_cost[(std::size_t)d * num_trks + t] = blob_match_cost(
            detections[d], m_predictions[t], s.use_point_count, s.point_count_weight);

    m_solver.solve(m_cost.data(), num_dets, num_trks);

    // The assignment is over a padded square matrix, so a pairing is only real
    // if the two are actually close enough.
    const std::vector<int>& row_match = m_solver.row_match();
    for(int d = 0; d < num_dets; d++)
    {
      const int t = row_match[d];
      if(t < 0 || t >= num_trks)
        continue;
      if(blob_dist(detections[d], m_predictions[t]) > m_match_dist)
        continue;

      m_matches.push_back({d, t});
      m_det_matched[d] = 1;
      m_trk_matched[t] = 1;
    }
  }

  for(const auto& m : m_matches)
    m_trackers[m.second].update(detections[m.first]);

  if(s.merge_memory_frames > 0)
    absorb_unmatched_tracks(detections, s);

  recover_split_tracks(detections, s);

  for(int d = 0; d < num_dets; d++)
  {
    if(m_det_matched[d])
      continue;
    KalmanTracker3D trk;
    trk.init(detections[d], m_next_id++);
    m_trackers.push_back(trk);
  }

  for(std::size_t i = 0; i < m_merge_memories.size();)
  {
    if(++m_merge_memories[i].age > s.merge_memory_frames)
      m_merge_memories.erase(m_merge_memories.begin() + i);
    else
      i++;
  }

  m_trackers.erase(
      std::remove_if(
          m_trackers.begin(), m_trackers.end(),
          [&](const KalmanTracker3D& t) { return t.time_since_update > s.max_age; }),
      m_trackers.end());

  m_tracked.clear();
  for(const auto& trk : m_trackers)
  {
    const bool updated_this_frame = trk.time_since_update == 0;
    const bool established = trk.hit_streak >= s.min_hits || m_frame_count <= s.min_hits;
    if(updated_this_frame && established)
      m_tracked.push_back(trk.state());
  }
}

// When two blobs merge, one track loses its detection. Rather than let its id
// die, park it against whichever matched track most plausibly swallowed it.
void Tracker::absorb_unmatched_tracks(
    const std::vector<Blob>& detections, const TrackerSettings& s)
{
  const int num_trks = (int)m_trackers.size();

  for(int t = 0; t < num_trks; t++)
  {
    if(m_trk_matched[t])
      continue;

    const Blob& pred_t = m_predictions[t];
    float best_dist = FLT_MAX;
    int best_tracker = -1;

    for(const auto& m : m_matches)
    {
      const Blob& absorbed = detections[m.first];
      const float threshold = (pred_t.radius() + absorbed.radius()) * 2.f;
      const float dist = blob_dist(pred_t, absorbed);
      if(dist < threshold && dist < best_dist)
      {
        best_dist = dist;
        best_tracker = m.second;
      }
    }

    if(best_tracker < 0)
      continue;

    const int32_t stored_id = m_trackers[t].id;

    bool already_stored = false;
    for(const auto& existing : m_merge_memories)
    {
      if(existing.stored_id == stored_id)
      {
        already_stored = true;
        break;
      }
    }

    if(!already_stored)
    {
      MergeMemory mm;
      mm.stored_id = stored_id;
      mm.last_cx = pred_t.cx;
      mm.last_cy = pred_t.cy;
      mm.last_cz = pred_t.cz;
      mm.last_vx = pred_t.vx;
      mm.last_vy = pred_t.vy;
      mm.last_vz = pred_t.vz;
      mm.half_extent[0] = std::max(m_trackers[t].kf_hx.x[0], 0.001f);
      mm.half_extent[1] = std::max(m_trackers[t].kf_hy.x[0], 0.001f);
      mm.half_extent[2] = std::max(m_trackers[t].kf_hz.x[0], 0.001f);
      mm.last_point_count = m_trackers[t].last_point_count;
      mm.absorber_id = m_trackers[best_tracker].id;
      m_merge_memories.push_back(mm);
    }

    // Retire the track now; the id lives on in the merge memory.
    m_trackers[t].time_since_update = s.max_age + 1;
  }
}

// The other half of merge handling: an unmatched detection near a remembered
// track is most likely that track reappearing, so give it its old id back.
void Tracker::recover_split_tracks(
    const std::vector<Blob>& detections, const TrackerSettings& s)
{
  const int num_dets = (int)detections.size();

  for(int d = 0; d < num_dets; d++)
  {
    if(m_det_matched[d])
      continue;

    int best_mem = -1;
    float best_score = FLT_MAX;

    for(int mi = 0; mi < (int)m_merge_memories.size(); mi++)
    {
      const MergeMemory& mem = m_merge_memories[mi];

      Blob remembered;
      remembered.cx = mem.last_cx;
      remembered.cy = mem.last_cy;
      remembered.cz = mem.last_cz;

      // Search around the absorber while it is alive, since the pair have been
      // moving together; fall back to the last known position once the absorber
      // is gone too.
      const KalmanTracker3D* absorber = nullptr;
      for(const auto& trk : m_trackers)
      {
        if(trk.id == mem.absorber_id)
        {
          absorber = &trk;
          break;
        }
      }

      if(absorber)
      {
        const Blob state = absorber->state();
        if(blob_dist(detections[d], state) > state.radius() * 3.f)
          continue;
      }
      else if(blob_dist(detections[d], remembered) > m_match_dist * 2.f)
      {
        continue;
      }

      const float mem_vol = std::max(
          mem.half_extent[0] * mem.half_extent[1] * mem.half_extent[2] * 8.f, 1e-10f);
      const float vol_ratio = detections[d].volume() / mem_vol;
      if(vol_ratio < 0.2f || vol_ratio > 5.f)
        continue;

      float score = std::abs(std::log(std::max(vol_ratio, 0.01f)))
                    + (float)mem.age * 0.01f
                    + blob_dist(detections[d], remembered) * 0.5f;

      if(s.use_point_count && s.point_count_weight > 0.f && mem.last_point_count > 0
         && detections[d].point_count > 0)
      {
        const float ratio
            = (float)std::min(detections[d].point_count, mem.last_point_count)
              / (float)std::max(detections[d].point_count, mem.last_point_count);
        score += (1.f - ratio) * s.point_count_weight;
      }

      if(score < best_score)
      {
        best_score = score;
        best_mem = mi;
      }
    }

    // A loose match is worse than a new id, so only recover a confident one.
    if(best_mem < 0 || best_score >= 10.f)
      continue;

    const int32_t recovered_id = m_merge_memories[best_mem].stored_id;

    bool id_in_use = false;
    for(const auto& trk : m_trackers)
    {
      if(trk.id == recovered_id && trk.time_since_update <= s.max_age)
      {
        id_in_use = true;
        break;
      }
    }

    if(!id_in_use)
    {
      KalmanTracker3D trk;
      trk.init(detections[d], recovered_id);
      m_trackers.push_back(trk);
      m_det_matched[d] = 1;
    }

    m_merge_memories.erase(m_merge_memories.begin() + best_mem);
  }
}

// Exponential smoothing on top of the Kalman output, for when a visibly calmer
// box matters more than tracking latency.
void Tracker::apply_smoothing(float smoothing)
{
  m_smoothing_active = smoothing > 0.f;
  if(!m_smoothing_active)
  {
    m_smoothed = m_tracked;
    return;
  }

  std::unordered_map<int32_t, const Blob*> previous;
  previous.reserve(m_smoothed.size());
  for(const auto& sb : m_smoothed)
    previous.emplace(sb.id, &sb);

  const float t = std::max(1.f - smoothing, 0.02f);

  std::vector<Blob> next;
  next.reserve(m_tracked.size());

  for(const auto& tracked : m_tracked)
  {
    Blob b = tracked;
    const auto it = previous.find(tracked.id);
    if(it != previous.end())
    {
      const Blob& p = *it->second;
      const float k = 1.f - t;
      b.cx = p.cx * k + tracked.cx * t;
      b.cy = p.cy * k + tracked.cy * t;
      b.cz = p.cz * k + tracked.cz * t;
      b.bmnx = p.bmnx * k + tracked.bmnx * t;
      b.bmny = p.bmny * k + tracked.bmny * t;
      b.bmnz = p.bmnz * k + tracked.bmnz * t;
      b.bmxx = p.bmxx * k + tracked.bmxx * t;
      b.bmxy = p.bmxy * k + tracked.bmxy * t;
      b.bmxz = p.bmxz * k + tracked.bmxz * t;
      b.vx = p.vx * k + tracked.vx * t;
      b.vy = p.vy * k + tracked.vy * t;
      b.vz = p.vz * k + tracked.vz * t;
    }
    next.push_back(b);
  }

  m_smoothed.swap(next);
}

void build_wireframe(
    const std::vector<Blob>& blobs, float velocity_threshold,
    std::vector<float>& vertices, std::vector<uint32_t>& indices)
{
  vertices.clear();
  indices.clear();
  if(blobs.empty())
    return;

  vertices.reserve(blobs.size() * points_per_blob * 4);
  indices.reserve(blobs.size() * lines_per_blob * 2);

  static constexpr int box_edges[12][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0}, // bottom face
      {4, 5}, {5, 6}, {6, 7}, {7, 4}, // top face
      {0, 4}, {1, 5}, {2, 6}, {3, 7}, // verticals
  };

  for(const auto& blob : blobs)
  {
    const uint32_t base = (uint32_t)(vertices.size() / 4);
    const float id = (float)blob.id;

    const auto emit = [&](float x, float y, float z) {
      vertices.push_back(x);
      vertices.push_back(y);
      vertices.push_back(z);
      vertices.push_back(id);
    };

    emit(blob.cx, blob.cy, blob.cz);

    const float corners[8][3] = {
        {blob.bmnx, blob.bmny, blob.bmnz}, {blob.bmxx, blob.bmny, blob.bmnz},
        {blob.bmxx, blob.bmxy, blob.bmnz}, {blob.bmnx, blob.bmxy, blob.bmnz},
        {blob.bmnx, blob.bmny, blob.bmxz}, {blob.bmxx, blob.bmny, blob.bmxz},
        {blob.bmxx, blob.bmxy, blob.bmxz}, {blob.bmnx, blob.bmxy, blob.bmxz},
    };
    for(const auto& c : corners)
      emit(c[0], c[1], c[2]);

    // Velocity arrow. It keeps a short stub at rest so a stationary blob still
    // shows which way it last faced.
    const float speed
        = std::sqrt(blob.vx * blob.vx + blob.vy * blob.vy + blob.vz * blob.vz);
    const float radius = std::max(blob.radius(), 0.001f);
    const float length = radius * 0.6f + std::min(speed * 10.f, radius * 2.4f);

    float tip[3]{blob.cx, blob.cy, blob.cz};
    float wing_l[3]{blob.cx, blob.cy, blob.cz};
    float wing_r[3]{blob.cx, blob.cy, blob.cz};

    if(speed > velocity_threshold)
    {
      const float inv = 1.f / speed;
      const float dx = blob.vx * inv, dy = blob.vy * inv, dz = blob.vz * inv;

      tip[0] = blob.cx + dx * length;
      tip[1] = blob.cy + dy * length;
      tip[2] = blob.cz + dz * length;

      // Any axis not parallel to the direction gives a usable barb plane.
      float ux = 0.f, uy = 1.f, uz = 0.f;
      if(std::abs(dy) > 0.9f)
      {
        ux = 1.f;
        uy = 0.f;
      }

      float px = dy * uz - dz * uy;
      float py = dz * ux - dx * uz;
      float pz = dx * uy - dy * ux;
      const float pm = std::sqrt(px * px + py * py + pz * pz);
      if(pm > 0.0001f)
      {
        px /= pm;
        py /= pm;
        pz /= pm;
      }

      const float spread = length * 0.3f;
      const float back = length * 0.25f;

      wing_l[0] = tip[0] - dx * back + px * spread;
      wing_l[1] = tip[1] - dy * back + py * spread;
      wing_l[2] = tip[2] - dz * back + pz * spread;
      wing_r[0] = tip[0] - dx * back - px * spread;
      wing_r[1] = tip[1] - dy * back - py * spread;
      wing_r[2] = tip[2] - dz * back - pz * spread;
    }

    emit(tip[0], tip[1], tip[2]);
    emit(wing_l[0], wing_l[1], wing_l[2]);
    emit(wing_r[0], wing_r[1], wing_r[2]);

    const uint32_t centroid = base;
    const uint32_t corner = base + 1;
    const uint32_t arrow_tip = base + 9;
    const uint32_t left = base + 10;
    const uint32_t right = base + 11;

    for(const auto& e : box_edges)
    {
      indices.push_back(corner + e[0]);
      indices.push_back(corner + e[1]);
    }

    indices.push_back(centroid);
    indices.push_back(arrow_tip);
    indices.push_back(arrow_tip);
    indices.push_back(left);
    indices.push_back(arrow_tip);
    indices.push_back(right);
  }
}

}

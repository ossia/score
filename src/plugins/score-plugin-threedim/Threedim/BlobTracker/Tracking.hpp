#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// SORT tracking: a constant-velocity Kalman filter per blob, and Hungarian
// assignment on a cost of centroid distance discounted by bounding-box overlap.
//
// Ported from the 3D Blob Tracker TouchDesigner POP (src/Tracking.h). It works
// on blob counts in the tens to hundreds, so this stayed on the CPU there too.

#include <Threedim/BlobTracker/Clustering.hpp>

#include <cmath>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <vector>

namespace Threedim::Blobs
{

struct Blob
{
  int32_t id = -1;
  float cx = 0, cy = 0, cz = 0;
  float bmnx = 0, bmny = 0, bmnz = 0;
  float bmxx = 0, bmxy = 0, bmxz = 0;
  int32_t point_count = 0;
  int32_t frames_since_last_seen = 0;
  float vx = 0, vy = 0, vz = 0;

  static Blob from_detection(const Detection& d) noexcept
  {
    Blob b;
    b.cx = d.cx;
    b.cy = d.cy;
    b.cz = d.cz;
    b.bmnx = d.bmnx;
    b.bmny = d.bmny;
    b.bmnz = d.bmnz;
    b.bmxx = d.bmxx;
    b.bmxy = d.bmxy;
    b.bmxz = d.bmxz;
    b.point_count = d.point_count;
    return b;
  }

  float radius() const noexcept
  {
    return std::max(std::max(bmxx - bmnx, bmxy - bmny), bmxz - bmnz) * 0.5f;
  }

  float volume() const noexcept { return (bmxx - bmnx) * (bmxy - bmny) * (bmxz - bmnz); }
};

inline float blob_dist(const Blob& a, const Blob& b) noexcept
{
  const float dx = a.cx - b.cx;
  const float dy = a.cy - b.cy;
  const float dz = a.cz - b.cz;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Centroid distance discounted by box overlap, so a detection that overlaps a
// prediction is preferred over an equally distant one that does not. Optionally
// penalises a mismatch in point count, which separates two people walking past
// each other when their boxes overlap but their densities differ.
inline float blob_match_cost(
    const Blob& det, const Blob& pred, bool use_point_count,
    float point_count_weight) noexcept
{
  const float dist = blob_dist(det, pred);

  const float ix
      = std::max(0.f, std::min(det.bmxx, pred.bmxx) - std::max(det.bmnx, pred.bmnx));
  const float iy
      = std::max(0.f, std::min(det.bmxy, pred.bmxy) - std::max(det.bmny, pred.bmny));
  const float iz
      = std::max(0.f, std::min(det.bmxz, pred.bmxz) - std::max(det.bmnz, pred.bmnz));

  const float inter = ix * iy * iz;
  const float union_vol = det.volume() + pred.volume() - inter;
  const float iou = (union_vol > 1e-10f) ? (inter / union_vol) : 0.f;

  float cost = dist * (1.f - iou * 0.5f);

  if(use_point_count && point_count_weight > 0.f && det.point_count > 0
     && pred.point_count > 0)
  {
    const float ratio = (float)std::min(det.point_count, pred.point_count)
                        / (float)std::max(det.point_count, pred.point_count);
    cost += (1.f - ratio) * point_count_weight * dist;
  }

  return cost;
}

// Scalar Kalman filter on [position, velocity] with a constant-velocity model
// and a unit time step, so predict() folds down to x += v.
struct KalmanFilter1D
{
  float x[2]{};
  float P[2][2]{};
  float Q[2][2]{};
  float R{};

  void init(float pos, float proc_noise, float meas_noise) noexcept
  {
    x[0] = pos;
    x[1] = 0.f;
    P[0][0] = 10.f;
    P[0][1] = 0.f;
    P[1][0] = 0.f;
    P[1][1] = 1000.f;
    Q[0][0] = proc_noise;
    Q[0][1] = 0.f;
    Q[1][0] = 0.f;
    Q[1][1] = proc_noise * 0.1f;
    R = meas_noise;
  }

  float predict() noexcept
  {
    x[0] += x[1];

    const float p00 = P[0][0] + P[1][0] + P[0][1] + P[1][1] + Q[0][0];
    const float p01 = P[0][1] + P[1][1] + Q[0][1];
    const float p10 = P[1][0] + P[1][1] + Q[1][0];
    const float p11 = P[1][1] + Q[1][1];

    P[0][0] = p00;
    P[0][1] = p01;
    P[1][0] = p10;
    P[1][1] = p11;
    return x[0];
  }

  void update(float z) noexcept
  {
    const float y = z - x[0];
    float S = P[0][0] + R;
    if(std::abs(S) < 1e-12f)
      S = 1e-12f;

    const float K0 = P[0][0] / S;
    const float K1 = P[1][0] / S;

    x[0] += K0 * y;
    x[1] += K1 * y;

    const float p00 = (1.f - K0) * P[0][0];
    const float p01 = (1.f - K0) * P[0][1];
    const float p10 = P[1][0] - K1 * P[0][0];
    const float p11 = P[1][1] - K1 * P[0][1];

    P[0][0] = p00;
    P[0][1] = p01;
    P[1][0] = p10;
    P[1][1] = p11;
  }
};

// Six independent scalar filters: centroid xyz and box half-extents xyz. The
// axes are treated as uncorrelated, which costs nothing in accuracy here and
// keeps the update free of matrix inversions.
struct KalmanTracker3D
{
  KalmanFilter1D kf_cx, kf_cy, kf_cz;
  KalmanFilter1D kf_hx, kf_hy, kf_hz;

  int32_t id = -1;
  int32_t time_since_update = 0;
  int32_t hits = 0;
  int32_t hit_streak = 0;
  int32_t age = 0;
  int32_t last_point_count = 0;

  void init(const Blob& det, int32_t track_id) noexcept
  {
    kf_cx.init(det.cx, 0.01f, 1.f);
    kf_cy.init(det.cy, 0.01f, 1.f);
    kf_cz.init(det.cz, 0.01f, 1.f);
    kf_hx.init((det.bmxx - det.bmnx) * 0.5f, 0.005f, 5.f);
    kf_hy.init((det.bmxy - det.bmny) * 0.5f, 0.005f, 5.f);
    kf_hz.init((det.bmxz - det.bmnz) * 0.5f, 0.005f, 5.f);

    id = track_id;
    time_since_update = 0;
    hits = 1;
    hit_streak = 1;
    age = 1;
    last_point_count = det.point_count;
  }

  Blob predict() noexcept
  {
    const float cx = kf_cx.predict();
    const float cy = kf_cy.predict();
    const float cz = kf_cz.predict();
    const float hx = std::max(kf_hx.predict(), 0.001f);
    const float hy = std::max(kf_hy.predict(), 0.001f);
    const float hz = std::max(kf_hz.predict(), 0.001f);

    age++;
    if(time_since_update > 0)
      hit_streak = 0;
    time_since_update++;

    return box(cx, cy, cz, hx, hy, hz);
  }

  void update(const Blob& det) noexcept
  {
    time_since_update = 0;
    hits++;
    hit_streak++;
    last_point_count = det.point_count;

    kf_cx.update(det.cx);
    kf_cy.update(det.cy);
    kf_cz.update(det.cz);
    kf_hx.update((det.bmxx - det.bmnx) * 0.5f);
    kf_hy.update((det.bmxy - det.bmny) * 0.5f);
    kf_hz.update((det.bmxz - det.bmnz) * 0.5f);
  }

  Blob state() const noexcept
  {
    return box(
        kf_cx.x[0], kf_cy.x[0], kf_cz.x[0], std::max(kf_hx.x[0], 0.001f),
        std::max(kf_hy.x[0], 0.001f), std::max(kf_hz.x[0], 0.001f));
  }

private:
  Blob box(float cx, float cy, float cz, float hx, float hy, float hz) const noexcept
  {
    Blob b;
    b.id = id;
    b.cx = cx;
    b.cy = cy;
    b.cz = cz;
    b.bmnx = cx - hx;
    b.bmny = cy - hy;
    b.bmnz = cz - hz;
    b.bmxx = cx + hx;
    b.bmxy = cy + hy;
    b.bmxz = cz + hz;
    b.vx = kf_cx.x[1];
    b.vy = kf_cy.x[1];
    b.vz = kf_cz.x[1];
    b.point_count = last_point_count;
    b.frames_since_last_seen = time_since_update;
    return b;
  }
};

// Remembers a track that was absorbed into another so its id can be handed back
// when the pair separates again.
struct MergeMemory
{
  int32_t stored_id = -1;
  float last_cx = 0, last_cy = 0, last_cz = 0;
  float last_vx = 0, last_vy = 0, last_vz = 0;
  float half_extent[3]{0, 0, 0};
  int32_t last_point_count = 0;
  int32_t absorber_id = -1;
  int32_t age = 0;
};

// Jonker-Volgenant shortest augmenting path over a square padded matrix. Holds
// its scratch across calls so a per-frame solve allocates nothing.
class HungarianSolver
{
public:
  // cost is num_rows x num_cols, row major. Afterwards row_match()[r] is the
  // column assigned to row r, or -1.
  void solve(const float* cost, int num_rows, int num_cols)
  {
    m_row_match.assign(std::max(num_rows, 0), -1);
    if(num_rows <= 0 || num_cols <= 0)
      return;

    const int n = std::max(num_rows, num_cols);

    m_cost.assign((size_t)n * n, 0.f);
    for(int r = 0; r < num_rows; r++)
      for(int c = 0; c < num_cols; c++)
        m_cost[(size_t)r * n + c] = cost[(size_t)r * num_cols + c];

    m_u.assign(n + 1, 0.f);
    m_v.assign(n + 1, 0.f);
    m_p.assign(n + 1, 0);
    m_way.assign(n + 1, 0);
    m_minv.resize(n + 1);
    m_used.resize(n + 1);

    for(int row = 1; row <= n; row++)
    {
      m_p[0] = row;
      int j0 = 0;

      std::fill(m_minv.begin(), m_minv.end(), FLT_MAX);
      std::fill(m_used.begin(), m_used.end(), (char)0);

      do
      {
        m_used[j0] = 1;

        const int i0 = m_p[j0];
        float delta = FLT_MAX;
        int j1 = 0;

        for(int j = 1; j <= n; j++)
        {
          if(m_used[j])
            continue;

          const float cur = m_cost[(size_t)(i0 - 1) * n + (j - 1)] - m_u[i0] - m_v[j];
          if(cur < m_minv[j])
          {
            m_minv[j] = cur;
            m_way[j] = j0;
          }
          if(m_minv[j] < delta)
          {
            delta = m_minv[j];
            j1 = j;
          }
        }

        for(int j = 0; j <= n; j++)
        {
          if(m_used[j])
          {
            m_u[m_p[j]] += delta;
            m_v[j] -= delta;
          }
          else
          {
            m_minv[j] -= delta;
          }
        }

        j0 = j1;
      } while(m_p[j0] != 0);

      do
      {
        const int j1 = m_way[j0];
        m_p[j0] = m_p[j1];
        j0 = j1;
      } while(j0);
    }

    // Drop pairings that only involve the padding.
    for(int j = 1; j <= num_cols; j++)
    {
      const int row = m_p[j];
      if(row >= 1 && row <= num_rows)
        m_row_match[row - 1] = j - 1;
    }
  }

  const std::vector<int>& row_match() const noexcept { return m_row_match; }

private:
  std::vector<float> m_cost;
  std::vector<float> m_u, m_v, m_minv;
  std::vector<int> m_p, m_way, m_row_match;
  std::vector<char> m_used;
};

}

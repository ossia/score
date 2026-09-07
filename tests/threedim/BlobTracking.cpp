// Threedim::Blobs::Tracker — the SORT tracking half of the 3D Blob Tracker port
// (Threedim/BlobTracker/Tracking.hpp + Tracker.cpp).
//
// Three things are worth pinning down here, because they are what a blob
// tracker is actually for:
//   - the Hungarian assignment is optimal, not merely a greedy nearest match
//     (checked against exhaustive permutation search);
//   - an id survives motion, and survives a gap shorter than Max track age;
//   - an id survives a merge and comes back on the split, which is the whole
//     point of the merge memory.
//
// No GPU, no Qt.

#include <Threedim/BlobTracker/Tracker.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <vector>

using Catch::Approx;
using namespace Threedim::Blobs;

namespace
{
Blob box_at(float cx, float cy, float cz, float half = 0.25f, int points = 500)
{
  Blob b;
  b.cx = cx;
  b.cy = cy;
  b.cz = cz;
  b.bmnx = cx - half;
  b.bmny = cy - half;
  b.bmnz = cz - half;
  b.bmxx = cx + half;
  b.bmxy = cy + half;
  b.bmxz = cz + half;
  b.point_count = points;
  return b;
}

TrackerSettings manual_settings()
{
  TrackerSettings s;
  s.auto_scale = false; // drive cluster/match distance from the controls
  s.cluster_dist = 0.1f;
  s.match_dist = 1.0f;
  s.min_points = 10;
  s.max_blobs = 64;
  s.max_age = 5;
  s.min_hits = 3;
  s.merge_memory_frames = 30;
  s.smoothing = 0.f;
  s.velocity_threshold = 0.001f;
  return s;
}

// Exhaustive optimum over all injective row->column assignments, including the
// choice of *which* rows are paired when there are more rows than columns. The
// solver pads the matrix to a square with zero-cost slots, so brute-forcing the
// same padded square asks it exactly the same question.
float brute_force_cost(const std::vector<float>& cost, int rows, int cols)
{
  const int n = std::max(rows, cols);
  std::vector<float> padded((std::size_t)n * n, 0.f);
  for(int r = 0; r < rows; r++)
    for(int c = 0; c < cols; c++)
      padded[(std::size_t)r * n + c] = cost[(std::size_t)r * cols + c];

  std::vector<int> perm(n);
  std::iota(perm.begin(), perm.end(), 0);

  float best = std::numeric_limits<float>::max();
  do
  {
    float total = 0.f;
    for(int r = 0; r < n; r++)
      total += padded[(std::size_t)r * n + perm[r]];
    best = std::min(best, total);
  } while(std::next_permutation(perm.begin(), perm.end()));
  return best;
}

const Blob* find_id(const std::vector<Blob>& blobs, int id)
{
  for(const auto& b : blobs)
    if(b.id == id)
      return &b;
  return nullptr;
}
}

TEST_CASE("Hungarian assignment is optimal", "[blobs]")
{
  std::mt19937 rng{2024};
  std::uniform_real_distribution<float> dist(0.f, 10.f);

  HungarianSolver solver;

  // Square, and both rectangular shapes: the solver pads to a square matrix,
  // so the padding must not leak into the reported pairings.
  for(auto [rows, cols] : {std::pair{4, 4}, std::pair{3, 5}, std::pair{5, 3}})
  {
    for(int trial = 0; trial < 30; trial++)
    {
      std::vector<float> cost((std::size_t)rows * cols);
      for(auto& v : cost)
        v = dist(rng);

      solver.solve(cost.data(), rows, cols);
      const auto& match = solver.row_match();
      REQUIRE((int)match.size() == rows);

      std::set<int> used;
      float total = 0.f;
      int assigned = 0;
      for(int r = 0; r < rows; r++)
      {
        if(match[r] < 0)
          continue;
        CHECK(match[r] < cols);
        CHECK(used.insert(match[r]).second); // no column assigned twice
        total += cost[(std::size_t)r * cols + match[r]];
        assigned++;
      }

      CHECK(assigned == std::min(rows, cols));
      CHECK(total == Approx(brute_force_cost(cost, rows, cols)).epsilon(1e-4));
    }
  }
}

TEST_CASE("Hungarian handles empty inputs", "[blobs]")
{
  HungarianSolver solver;
  float dummy = 0.f;
  solver.solve(&dummy, 0, 0);
  CHECK(solver.row_match().empty());
  solver.solve(&dummy, 2, 0);
  CHECK(solver.row_match().size() == 2);
  CHECK(solver.row_match()[0] == -1);
}

TEST_CASE("A moving blob keeps one id", "[blobs]")
{
  Tracker t;
  const auto s = manual_settings();

  int id = -1;
  for(int frame = 0; frame < 40; frame++)
  {
    std::vector<Blob> dets{box_at(frame * 0.1f, 0.f, 0.f)};
    t.track(dets, s);

    if(frame < s.min_hits - 1)
      continue;

    REQUIRE(t.blobs().size() == 1);
    if(id < 0)
      id = t.blobs()[0].id;
    CHECK(t.blobs()[0].id == id);
  }

  CHECK(id == 0);
  CHECK(t.next_id() == 1);

  // The constant-velocity filter has learned the motion.
  CHECK(t.blobs()[0].vx == Approx(0.1f).epsilon(0.15));
  CHECK(t.blobs()[0].cx == Approx(39 * 0.1f).margin(0.05));
}

TEST_CASE("Two blobs keep distinct ids as they cross paths", "[blobs]")
{
  Tracker t;
  auto s = manual_settings();
  s.merge_memory_frames = 0; // isolate plain association from merge handling

  std::set<int> seen;
  for(int frame = 0; frame < 30; frame++)
  {
    const float x = frame * 0.1f;
    std::vector<Blob> dets{box_at(-2.f + x, 0.f, 0.f), box_at(2.f - x, 0.f, 0.f)};
    t.track(dets, s);

    if(frame < 2)
      continue;
    REQUIRE(t.blobs().size() == 2);
    CHECK(t.blobs()[0].id != t.blobs()[1].id);
    seen.insert(t.blobs()[0].id);
    seen.insert(t.blobs()[1].id);
  }

  CHECK(seen.size() == 2); // no id churn over 30 frames
}

TEST_CASE("A track survives a gap shorter than Max track age", "[blobs]")
{
  Tracker t;
  auto s = manual_settings();
  s.max_age = 5;
  s.min_hits = 1;

  for(int frame = 0; frame < 6; frame++)
    t.track({box_at(0.f, 0.f, 0.f)}, s);

  REQUIRE(t.blobs().size() == 1);
  const int id = t.blobs()[0].id;

  // Three frames of nothing: under Max track age, so the track lives on even
  // though it reports nothing.
  for(int frame = 0; frame < 3; frame++)
  {
    t.track({}, s);
    CHECK(t.blobs().empty());
  }
  CHECK(t.live_track_count() == 1);

  t.track({box_at(0.f, 0.f, 0.f)}, s);
  REQUIRE(t.blobs().size() == 1);
  CHECK(t.blobs()[0].id == id);
}

TEST_CASE("A track dies once it is older than Max track age", "[blobs]")
{
  Tracker t;
  auto s = manual_settings();
  s.max_age = 3;
  s.min_hits = 1;
  s.merge_memory_frames = 0;

  for(int frame = 0; frame < 6; frame++)
    t.track({box_at(0.f, 0.f, 0.f)}, s);
  const int id = t.blobs()[0].id;

  for(int frame = 0; frame < 10; frame++)
    t.track({}, s);

  CHECK(t.live_track_count() == 0);

  // The same blob reappearing is a new object as far as the tracker knows.
  for(int frame = 0; frame < 3; frame++)
    t.track({box_at(0.f, 0.f, 0.f)}, s);
  REQUIRE(t.blobs().size() == 1);
  CHECK(t.blobs()[0].id != id);
}

TEST_CASE("Min hit streak gates a fresh track", "[blobs]")
{
  Tracker t;
  auto s = manual_settings();
  s.min_hits = 3;

  // Warm up past the frame count grace period with one blob.
  for(int frame = 0; frame < 8; frame++)
    t.track({box_at(0.f, 0.f, 0.f)}, s);
  REQUIRE(t.blobs().size() == 1);

  // A second blob appears: not reported until it has been seen min_hits times.
  t.track({box_at(0.f, 0.f, 0.f), box_at(5.f, 0.f, 0.f)}, s);
  CHECK(t.blobs().size() == 1);
  t.track({box_at(0.f, 0.f, 0.f), box_at(5.f, 0.f, 0.f)}, s);
  CHECK(t.blobs().size() == 1);
  t.track({box_at(0.f, 0.f, 0.f), box_at(5.f, 0.f, 0.f)}, s);
  CHECK(t.blobs().size() == 2);
}

TEST_CASE("A merged id is parked and handed back on the split", "[blobs]")
{
  // Two blobs walk together, become one detection for a while, then separate.
  // Without the merge memory the reappearing blob would take a fresh id.
  Tracker t;
  auto s = manual_settings();
  s.min_hits = 1;
  s.max_age = 2;
  s.merge_memory_frames = 60;
  s.match_dist = 1.5f;

  for(int frame = 0; frame < 8; frame++)
    t.track({box_at(-0.6f, 0.f, 0.f), box_at(0.6f, 0.f, 0.f)}, s);

  REQUIRE(t.blobs().size() == 2);
  std::vector<int> ids{t.blobs()[0].id, t.blobs()[1].id};
  std::sort(ids.begin(), ids.end());

  // Merged: one wide detection covering both.
  for(int frame = 0; frame < 6; frame++)
    t.track({box_at(0.f, 0.f, 0.f, 0.9f, 1000)}, s);

  CHECK(t.merge_memory_count() == 1);
  CHECK(t.blobs().size() == 1);

  // Split again.
  for(int frame = 0; frame < 4; frame++)
    t.track({box_at(-0.6f, 0.f, 0.f), box_at(0.6f, 0.f, 0.f)}, s);

  REQUIRE(t.blobs().size() == 2);
  std::vector<int> after{t.blobs()[0].id, t.blobs()[1].id};
  std::sort(after.begin(), after.end());
  CHECK(after == ids);
  CHECK(t.next_id() == 2); // nothing new was ever allocated
}

TEST_CASE("A merged id expires with the merge memory", "[blobs]")
{
  Tracker t;
  auto s = manual_settings();
  s.min_hits = 1;
  s.max_age = 2;
  s.merge_memory_frames = 3; // very short memory
  s.match_dist = 1.5f;

  for(int frame = 0; frame < 8; frame++)
    t.track({box_at(-0.6f, 0.f, 0.f), box_at(0.6f, 0.f, 0.f)}, s);
  const int allocated = t.next_id();
  REQUIRE(allocated == 2);

  for(int frame = 0; frame < 12; frame++)
    t.track({box_at(0.f, 0.f, 0.f, 0.9f, 1000)}, s);
  CHECK(t.merge_memory_count() == 0);

  for(int frame = 0; frame < 4; frame++)
    t.track({box_at(-0.6f, 0.f, 0.f), box_at(0.6f, 0.f, 0.f)}, s);

  REQUIRE(t.blobs().size() == 2);
  CHECK(t.next_id() > allocated); // the forgotten blob had to take a new id
}

TEST_CASE("Smoothing lags the raw estimate without changing ids", "[blobs]")
{
  auto run = [](float smoothing) {
    Tracker t;
    auto s = manual_settings();
    s.min_hits = 1;
    s.smoothing = smoothing;
    for(int frame = 0; frame < 12; frame++)
      t.track({box_at(frame * 0.2f, 0.f, 0.f)}, s);
    REQUIRE(t.blobs().size() == 1);
    return t.blobs()[0];
  };

  const Blob raw = run(0.f);
  const Blob smoothed = run(0.8f);

  CHECK(smoothed.id == raw.id);
  // Heavier smoothing trails a blob moving in +x.
  CHECK(smoothed.cx < raw.cx);
  CHECK(smoothed.cx > raw.cx - 2.f);
}

TEST_CASE("Reset clears every track and restarts ids", "[blobs]")
{
  Tracker t;
  auto s = manual_settings();
  s.min_hits = 1;

  for(int frame = 0; frame < 6; frame++)
    t.track({box_at(0.f, 0.f, 0.f), box_at(4.f, 0.f, 0.f)}, s);
  REQUIRE(t.blobs().size() == 2);
  REQUIRE(t.next_id() == 2);

  t.reset();
  CHECK(t.live_track_count() == 0);
  CHECK(t.merge_memory_count() == 0);
  CHECK(t.blobs().empty());
  CHECK(t.next_id() == 0);
  CHECK(t.frame_count() == 0);

  t.track({box_at(0.f, 0.f, 0.f)}, s);
  REQUIRE(t.blobs().size() == 1);
  CHECK(t.blobs()[0].id == 0);
}

TEST_CASE("End to end: a point cloud in, tracked blobs out", "[blobs]")
{
  // Two clouds of points sliding past each other, clustered and tracked through
  // the same entry point the score object uses.
  auto ball = [](std::vector<float>& pos, float cx, float cy, float cz, float radius,
                 int count, std::mt19937& rng) {
    std::uniform_real_distribution<float> unit(-1.f, 1.f);
    for(int i = 0; i < count; i++)
    {
      float x, y, z;
      do
      {
        x = unit(rng);
        y = unit(rng);
        z = unit(rng);
      } while(x * x + y * y + z * z > 1.f);
      pos.push_back(cx + x * radius);
      pos.push_back(cy + y * radius);
      pos.push_back(cz + z * radius);
    }
  };

  Tracker t;
  auto s = manual_settings();
  s.min_points = 40;
  s.min_hits = 2;
  s.cluster_dist = 0.09f;
  s.match_dist = 1.0f;

  std::mt19937 rng{4242};
  std::set<int> ids;
  for(int frame = 0; frame < 20; frame++)
  {
    std::vector<float> pos;
    ball(pos, -1.5f + frame * 0.1f, 0.f, 0.f, 0.2f, 300, rng);
    ball(pos, 1.5f, 0.f, 0.f, 0.2f, 300, rng);

    t.process(pos.data(), (int64_t)(pos.size() / 3), s);

    CHECK(t.cluster_result().num_valid_points == 600);
    CHECK(t.cluster_result().num_blobs == 2);

    if(frame < 2)
      continue;
    REQUIRE(t.blobs().size() == 2);
    for(const auto& b : t.blobs())
      ids.insert(b.id);
  }

  CHECK(ids.size() == 2);

  // An empty cloud is not an error: the tracks age out instead of freezing.
  for(int frame = 0; frame < 10; frame++)
    t.process(nullptr, 0, s);
  CHECK(t.blobs().empty());
  CHECK(t.live_track_count() == 0);
}

TEST_CASE("Auto scale derives the distances from the measured spacing", "[blobs]")
{
  // With Auto scale on, the cluster distance is the measured spacing times
  // scale_factor * 8, and the match distance is 5x that. The first frame has no
  // measurement and falls back to the controls.
  const float step = 0.05f;
  std::vector<float> pos;
  for(int i = 0; i < 20; i++)
    for(int j = 0; j < 20; j++)
      for(int k = 0; k < 20; k++)
      {
        pos.push_back(i * step);
        pos.push_back(j * step);
        pos.push_back(k * step);
      }

  Tracker t;
  TrackerSettings s;
  s.auto_scale = true;
  s.scale_factor = 0.25f;
  s.cluster_dist = 0.06f; // first-frame fallback
  s.match_dist = 0.4f;
  s.min_points = 50;
  s.min_hits = 1;

  t.process(pos.data(), (int64_t)(pos.size() / 3), s);
  CHECK(t.cluster_distance() == Approx(0.06f));
  CHECK(t.point_spacing() > 0.f);

  for(int frame = 0; frame < 20; frame++)
    t.process(pos.data(), (int64_t)(pos.size() / 3), s);

  CHECK(t.point_spacing() == Approx(step).epsilon(0.45));
  CHECK(
      t.cluster_distance()
      == Approx(t.point_spacing() * s.scale_factor * spacing_to_cluster_dist));
  CHECK(t.match_distance() == Approx(t.cluster_distance() * cluster_dist_to_match_dist));
}

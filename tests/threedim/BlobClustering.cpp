// Threedim::Blobs::Clusterer — the CPU port of the 3D Blob Tracker's OpenGL
// compute clustering (Threedim/BlobTracker/Clustering.*).
//
// The load-bearing check is the same one the original's headless test used: an
// independent brute-force connected-components pass over the same "closer than
// the cluster distance" rule, compared against what the spatial-hash + 27-cell
// union-find pipeline produces. Everything the grid does — the histogram, the
// prefix sum, the scatter into cell order, the hash — is an optimisation of
// that definition, so any bug in it shows up as a disagreement here.
//
// No GPU, no Qt: the clusterer is plain C++ over a float array.

#include <Threedim/BlobTracker/Clustering.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <algorithm>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

using Catch::Approx;
using namespace Threedim::Blobs;

namespace
{
// A ball of points around a centre, with a spacing well below the cluster
// distance so the partition does not hinge on floating-point ties.
void add_ball(
    std::vector<float>& pos, float cx, float cy, float cz, float radius, int count,
    std::mt19937& rng)
{
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
}

// Brute force connected components under the rule the union stage uses, as an
// independent answer to compare against.
std::vector<int>
reference_component_sizes(const std::vector<float>& pos, float dist, int min_points)
{
  const int n = (int)(pos.size() / 3);
  std::vector<int> parent(n);
  std::iota(parent.begin(), parent.end(), 0);

  auto find = [&](int x) {
    while(parent[x] != x)
    {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  };

  const float d2 = dist * dist;
  for(int i = 0; i < n; i++)
  {
    for(int j = i + 1; j < n; j++)
    {
      const float dx = pos[i * 3] - pos[j * 3];
      const float dy = pos[i * 3 + 1] - pos[j * 3 + 1];
      const float dz = pos[i * 3 + 2] - pos[j * 3 + 2];
      if(dx * dx + dy * dy + dz * dz >= d2)
        continue;

      const int a = find(i);
      const int b = find(j);
      if(a != b)
        parent[a] = b;
    }
  }

  std::vector<int> sizes(n, 0);
  for(int i = 0; i < n; i++)
    sizes[find(i)]++;

  std::vector<int> out;
  for(int s : sizes)
    if(s >= min_points)
      out.push_back(s);

  std::sort(out.begin(), out.end());
  return out;
}

std::vector<int> detected_sizes(const std::vector<Detection>& blobs)
{
  std::vector<int> out;
  out.reserve(blobs.size());
  for(const auto& b : blobs)
    out.push_back(b.point_count);
  std::sort(out.begin(), out.end());
  return out;
}

// A dense axis-aligned lattice: spacing is exactly `step` along each axis, so
// the k=6 nearest neighbours of an interior point all sit at that distance.
std::vector<float> lattice(int side, float step, float origin = 0.f)
{
  std::vector<float> pos;
  pos.reserve((size_t)side * side * side * 3);
  for(int i = 0; i < side; i++)
    for(int j = 0; j < side; j++)
      for(int k = 0; k < side; k++)
      {
        pos.push_back(origin + i * step);
        pos.push_back(origin + j * step);
        pos.push_back(origin + k * step);
      }
  return pos;
}
}

TEST_CASE("Clustering agrees with brute-force connected components", "[blobs]")
{
  std::mt19937 rng{1234};

  std::vector<float> pos;
  add_ball(pos, 0.f, 0.f, 0.f, 0.20f, 400, rng);
  add_ball(pos, 1.5f, 0.f, 0.f, 0.15f, 300, rng);
  add_ball(pos, 0.f, 1.5f, 0.6f, 0.25f, 500, rng);

  const float dist = 0.08f;
  const int min_points = 20;

  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;
  p.cluster_dist = dist;
  p.min_points = min_points;
  p.max_blobs = 100;

  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, blobs, res);

  CHECK(res.num_valid_points == 1200);
  CHECK(res.cluster_overflow == false);
  CHECK(detected_sizes(blobs) == reference_component_sizes(pos, dist, min_points));
  CHECK(blobs.size() == 3);
}

TEST_CASE("Clustering agrees with brute force on a saturating hash table", "[blobs]")
{
  // Cells this far apart (1e4 apart, cell size 0.05) are all over the hash
  // table, so buckets hold points from unrelated cells. The distance test is
  // what decides membership, so a collision may cost time but must never merge
  // two blobs — which is exactly what the brute-force comparison pins down.
  std::mt19937 rng{99};
  std::vector<float> pos;
  for(int i = 0; i < 8; i++)
    add_ball(pos, i * 1.0e4f, (i % 3) * 1.0e4f, (i % 2) * -1.0e4f, 0.10f, 120, rng);

  const float dist = 0.05f;

  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;
  p.cluster_dist = dist;
  p.min_points = 10;
  p.max_blobs = 100;

  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, blobs, res);

  CHECK(detected_sizes(blobs) == reference_component_sizes(pos, dist, 10));
  CHECK(blobs.size() == 8);
}

TEST_CASE("Non-finite points are skipped, not clustered", "[blobs]")
{
  // Depth sensors emit NaN and Inf for invalid pixels. On the GPU a NaN operand
  // made the atomic min/max compare-and-swap loops spin forever; here it would
  // poison a bounding box and every comparison against it.
  std::mt19937 rng{7};
  std::vector<float> pos;
  add_ball(pos, 0.f, 0.f, 0.f, 0.1f, 200, rng);

  const auto nan = std::numeric_limits<float>::quiet_NaN();
  const auto inf = std::numeric_limits<float>::infinity();
  for(auto bad : {nan, inf, -inf, 1e20f})
  {
    pos.push_back(bad);
    pos.push_back(0.f);
    pos.push_back(0.f);
  }
  add_ball(pos, 3.f, 0.f, 0.f, 0.1f, 150, rng);

  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;
  p.cluster_dist = 0.06f;
  p.min_points = 10;

  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, blobs, res);

  CHECK(res.num_valid_points == 350);
  REQUIRE(blobs.size() == 2);
  for(const auto& b : blobs)
  {
    CHECK(std::isfinite(b.cx));
    CHECK(std::isfinite(b.bmnx));
    CHECK(std::isfinite(b.bmxx));
    CHECK(b.point_count > 0);
  }
  CHECK(blobs[0].point_count + blobs[1].point_count == 350);
}

TEST_CASE("Min points filters components without hiding them from the count", "[blobs]")
{
  std::mt19937 rng{11};
  std::vector<float> pos;
  add_ball(pos, 0.f, 0.f, 0.f, 0.1f, 200, rng);
  add_ball(pos, 5.f, 0.f, 0.f, 0.1f, 12, rng); // below the filter

  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;
  p.cluster_dist = 0.06f;
  p.min_points = 50;

  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, blobs, res);

  CHECK(blobs.size() == 1);
  CHECK(res.num_blobs == 1);
  // num_clusters is the component count *before* the filter: two balls, plus
  // however many stragglers the small one broke into.
  CHECK(res.num_clusters >= 2);
}

TEST_CASE("Max blobs caps the output", "[blobs]")
{
  std::mt19937 rng{3};
  std::vector<float> pos;
  for(int i = 0; i < 10; i++)
    add_ball(pos, i * 3.f, 0.f, 0.f, 0.1f, 100, rng);

  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;
  p.cluster_dist = 0.06f;
  p.min_points = 20;
  p.max_blobs = 4;

  const int n = c.detect(pos.data(), (int64_t)(pos.size() / 3), p, blobs, res);

  CHECK(n == 4);
  CHECK(blobs.size() == 4);
  CHECK(res.num_blobs == 4);
  CHECK(res.num_clusters == 10);
}

TEST_CASE("Centroid and bounding box are exact at large coordinates", "[blobs]")
{
  // The GPU original accumulated centroids as integers quantised per frame
  // against the bounding box, because GPU atomics have no float add — a fixed
  // quantisation overflowed int32 at millimetre-scale coordinates. The port
  // sums in double, so a cloud centred 100km from the origin still resolves its
  // centroid to the millimetre.
  const float origin = 1.0e5f;
  const float step = 1.f;
  const int side = 11;
  auto pos = lattice(side, step, origin);

  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;
  p.cluster_dist = step * 1.5f;
  p.min_points = 10;

  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, blobs, res);

  REQUIRE(blobs.size() == 1);
  const auto& b = blobs[0];
  const float centre = origin + step * (side - 1) * 0.5f;

  CHECK(b.point_count == side * side * side);
  // Summing 1331 coordinates of ~1e5 in float32 would reach ~1.3e8, where the
  // representable spacing is 16 — the centroid would drift by ~0.1. Accumulating
  // in double keeps it exact.
  CHECK(b.cx == Approx(centre).margin(1e-2));
  CHECK(b.cy == Approx(centre).margin(1e-2));
  CHECK(b.cz == Approx(centre).margin(1e-2));
  CHECK(b.bmnx == Approx(origin).margin(1e-2));
  CHECK(b.bmxx == Approx(origin + step * (side - 1)).margin(1e-2));
  CHECK(b.bmnz == Approx(origin).margin(1e-2));
  CHECK(b.bmxz == Approx(origin + step * (side - 1)).margin(1e-2));
}

TEST_CASE("Blob order is deterministic and follows the input", "[blobs]")
{
  // The GPU handed out cluster ids by atomicAdd, so blob order varied between
  // runs. The port reports the clusters in the order their first point appears
  // in the input — cell order, which everything downstream of the scatter works
  // in, is hash order and would say nothing to whoever supplied the cloud.
  std::mt19937 rng{5};
  std::vector<float> pos;
  add_ball(pos, 0.f, 0.f, 0.f, 0.1f, 100, rng);  // first in the input
  add_ball(pos, 4.f, 0.f, 0.f, 0.1f, 200, rng);  // second
  add_ball(pos, -4.f, 0.f, 0.f, 0.1f, 300, rng); // third

  Clusterer c;
  ClusterParams p;
  p.cluster_dist = 0.06f;
  p.min_points = 20;

  std::vector<Detection> first, second;
  ClusterResult r1, r2;
  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, first, r1);
  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, second, r2);

  REQUIRE(first.size() == 3);
  REQUIRE(second.size() == 3);
  for(std::size_t i = 0; i < first.size(); i++)
  {
    CHECK(first[i].point_count == second[i].point_count);
    CHECK(first[i].cx == Approx(second[i].cx));
  }

  // Input order: ball at x=0 first, then x=4, then x=-4.
  CHECK(first[0].cx == Approx(0.f).margin(0.05));
  CHECK(first[1].cx == Approx(4.f).margin(0.05));
  CHECK(first[2].cx == Approx(-4.f).margin(0.05));
}

TEST_CASE("Empty and degenerate inputs are not errors", "[blobs]")
{
  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;

  CHECK(c.detect(nullptr, 100, p, blobs, res) == 0);
  CHECK(blobs.empty());

  std::vector<float> pos{1.f, 2.f, 3.f};
  CHECK(c.detect(pos.data(), 0, p, blobs, res) == 0);
  CHECK(res.num_valid_points == 0);

  // A cloud of nothing but invalid points.
  const auto nan = std::numeric_limits<float>::quiet_NaN();
  std::vector<float> junk{nan, nan, nan, nan, nan, nan};
  CHECK(c.detect(junk.data(), 2, p, blobs, res) == 0);
  CHECK(res.num_valid_points == 0);
  CHECK(res.num_clusters == 0);
  CHECK(blobs.empty());
}

TEST_CASE("Spacing estimate measures the point spacing", "[blobs]")
{
  // On a cubic lattice an interior point's 6 nearest neighbours are all exactly
  // one step away, so the k=6 estimate is the lattice step. The estimate is the
  // 75th percentile over a strided sample; a 30^3 lattice is 81% interior, so
  // that percentile lands on an interior point.
  const float step = 0.1f;
  auto pos = lattice(30, step);

  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;
  p.cluster_dist = step * 1.2f;
  p.min_points = 10;
  p.knn_k = 6;

  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, blobs, res);

  // Between the lattice step itself and the face-diagonal it would report if it
  // were counting the wrong neighbours.
  CHECK(res.knn_spacing >= 0.99f * step);
  CHECK(res.knn_spacing <= 1.42f * step);

  // The scale-free statement: the estimate is proportional to the spacing.
  auto coarse = lattice(30, step * 2.f);
  ClusterResult res2;
  ClusterParams p2 = p;
  p2.cluster_dist = step * 2.f * 1.2f;
  c.detect(coarse.data(), (int64_t)(coarse.size() / 3), p2, blobs, res2);

  CHECK(res2.knn_spacing == Approx(2.f * res.knn_spacing).epsilon(0.01));
}

TEST_CASE("Spacing estimate is skipped when not asked for", "[blobs]")
{
  auto pos = lattice(8, 0.1f);

  Clusterer c;
  std::vector<Detection> blobs;
  ClusterResult res;
  ClusterParams p;
  p.cluster_dist = 0.12f;
  p.min_points = 10;
  p.knn_k = 0;

  c.detect(pos.data(), (int64_t)(pos.size() / 3), p, blobs, res);
  CHECK(res.knn_spacing == 0.f);
}

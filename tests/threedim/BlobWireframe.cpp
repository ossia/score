// Threedim::Blobs::build_wireframe — the geometry the 3D Blob Tracker emits:
// per tracked blob, 12 vertices and 15 lines (the centroid, the 8 box corners,
// and a velocity arrow), with the track id riding along as a 4th float per
// vertex.
//
// The interesting invariants are the ones a typo in the corner table or the
// edge list would break: every box edge must join two corners that differ in
// exactly one axis, and the second blob's indices must be based off its own
// vertices rather than the first's.
//
// No GPU, no Qt.

#include <Threedim/BlobTracker/Tracker.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <set>
#include <vector>

using Catch::Approx;
using namespace Threedim::Blobs;

namespace
{
Blob blob_at(
    int id, float cx, float cy, float cz, float half, float vx = 0.f, float vy = 0.f,
    float vz = 0.f)
{
  Blob b;
  b.id = id;
  b.cx = cx;
  b.cy = cy;
  b.cz = cz;
  b.bmnx = cx - half;
  b.bmny = cy - half;
  b.bmnz = cz - half;
  b.bmxx = cx + half;
  b.bmxy = cy + half;
  b.bmxz = cz + half;
  b.vx = vx;
  b.vy = vy;
  b.vz = vz;
  return b;
}

struct V
{
  float x, y, z, id;
};

V vertex(const std::vector<float>& v, std::size_t i)
{
  return V{v[i * 4], v[i * 4 + 1], v[i * 4 + 2], v[i * 4 + 3]};
}
}

TEST_CASE("Wireframe emits 12 vertices and 15 lines per blob", "[blobs]")
{
  std::vector<float> vertices;
  std::vector<uint32_t> indices;

  build_wireframe({}, 0.001f, vertices, indices);
  CHECK(vertices.empty());
  CHECK(indices.empty());

  const std::vector<Blob> blobs{
      blob_at(7, 0.f, 0.f, 0.f, 0.5f), blob_at(9, 4.f, 1.f, -2.f, 0.25f)};
  build_wireframe(blobs, 0.001f, vertices, indices);

  CHECK(vertices.size() == blobs.size() * points_per_blob * 4);
  CHECK(indices.size() == blobs.size() * lines_per_blob * 2);

  // Every index is in range, and the second blob's lines reference the second
  // blob's vertices.
  for(auto i : indices)
    CHECK(i < vertices.size() / 4);
  for(std::size_t k = lines_per_blob * 2; k < indices.size(); k++)
    CHECK(indices[k] >= points_per_blob);
}

TEST_CASE("Every vertex carries its track id", "[blobs]")
{
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
  build_wireframe(
      {blob_at(3, 0.f, 0.f, 0.f, 0.5f), blob_at(11, 4.f, 0.f, 0.f, 0.5f)}, 0.001f,
      vertices, indices);

  for(int i = 0; i < points_per_blob; i++)
    CHECK(vertex(vertices, i).id == Approx(3.f));
  for(int i = points_per_blob; i < 2 * points_per_blob; i++)
    CHECK(vertex(vertices, i).id == Approx(11.f));
}

TEST_CASE(
    "The box corners span the bounding box and its edges are axis aligned", "[blobs]")
{
  const float half = 0.75f;
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
  build_wireframe({blob_at(0, 1.f, 2.f, 3.f, half)}, 0.001f, vertices, indices);

  // Vertex 0 is the centroid, 1..8 the corners.
  const V centroid = vertex(vertices, 0);
  CHECK(centroid.x == Approx(1.f));
  CHECK(centroid.y == Approx(2.f));
  CHECK(centroid.z == Approx(3.f));

  std::set<std::tuple<int, int, int>> corner_signs;
  for(int i = 1; i <= 8; i++)
  {
    const V v = vertex(vertices, i);
    CHECK(std::abs(v.x - 1.f) == Approx(half));
    CHECK(std::abs(v.y - 2.f) == Approx(half));
    CHECK(std::abs(v.z - 3.f) == Approx(half));
    corner_signs.insert({v.x > 1.f ? 1 : -1, v.y > 2.f ? 1 : -1, v.z > 3.f ? 1 : -1});
  }
  // All 8 sign combinations, i.e. the 8 distinct corners.
  CHECK(corner_signs.size() == 8);

  // The first 12 lines are the box edges: each joins two corners that differ on
  // exactly one axis, by the full extent.
  for(int e = 0; e < 12; e++)
  {
    const V a = vertex(vertices, indices[e * 2]);
    const V b = vertex(vertices, indices[e * 2 + 1]);
    const int dx = std::abs(a.x - b.x) > 1e-5f;
    const int dy = std::abs(a.y - b.y) > 1e-5f;
    const int dz = std::abs(a.z - b.z) > 1e-5f;
    CHECK(dx + dy + dz == 1);
    CHECK(
        std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z)
        == Approx(2.f * half));
  }
}

TEST_CASE("A blob below the velocity threshold gets a degenerate arrow", "[blobs]")
{
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
  build_wireframe(
      {blob_at(0, 1.f, 2.f, 3.f, 0.5f, 0.0001f, 0.f, 0.f)}, 0.01f, vertices, indices);

  // Vertices 9, 10, 11 are the tip and the two barb wings; at rest they all
  // collapse onto the centroid so the arrow draws as nothing.
  for(int i = 9; i <= 11; i++)
  {
    const V v = vertex(vertices, i);
    CHECK(v.x == Approx(1.f));
    CHECK(v.y == Approx(2.f));
    CHECK(v.z == Approx(3.f));
  }
}

TEST_CASE("The arrow points along the velocity", "[blobs]")
{
  const float half = 0.5f;
  const float vx = 0.03f;
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
  build_wireframe(
      {blob_at(0, 0.f, 0.f, 0.f, half, vx, 0.f, 0.f)}, 0.001f, vertices, indices);

  const V tip = vertex(vertices, 9);
  const V left = vertex(vertices, 10);
  const V right = vertex(vertices, 11);

  // radius * 0.6 + min(speed * 10, radius * 2.4), along +x.
  const float speed = vx;
  const float radius = half; // max extent / 2 for a cube of half-extent `half`
  const float length = radius * 0.6f + std::min(speed * 10.f, radius * 2.4f);

  CHECK(tip.x == Approx(length));
  CHECK(tip.y == Approx(0.f).margin(1e-6));
  CHECK(tip.z == Approx(0.f).margin(1e-6));
  CHECK(tip.x > 0.f);

  // The barbs sit behind the tip, symmetric about the velocity axis and
  // perpendicular to it.
  CHECK(left.x == Approx(right.x));
  CHECK(left.x < tip.x);
  CHECK(left.y == Approx(-right.y).margin(1e-6));
  CHECK(left.z == Approx(-right.z).margin(1e-6));
  const float spread = std::sqrt(
      (left.y - right.y) * (left.y - right.y) + (left.z - right.z) * (left.z - right.z));
  CHECK(spread == Approx(2.f * length * 0.3f));

  // The three arrow lines: centroid->tip, tip->left, tip->right.
  CHECK(indices[24] == 0u);
  CHECK(indices[25] == 9u);
  CHECK(indices[26] == 9u);
  CHECK(indices[27] == 10u);
  CHECK(indices[28] == 9u);
  CHECK(indices[29] == 11u);
}

TEST_CASE("A blob moving straight up still gets a usable barb plane", "[blobs]")
{
  // The barb plane comes from a cross product with an axis picked to not be
  // parallel to the direction; +y motion is the case that needs the fallback.
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
  build_wireframe(
      {blob_at(0, 0.f, 0.f, 0.f, 0.5f, 0.f, 0.05f, 0.f)}, 0.001f, vertices, indices);

  const V tip = vertex(vertices, 9);
  const V left = vertex(vertices, 10);
  const V right = vertex(vertices, 11);

  CHECK(tip.y > 0.f);
  CHECK(tip.x == Approx(0.f).margin(1e-6));

  // Distinct, finite, and symmetric: not collapsed by a zero-length cross.
  CHECK(std::isfinite(left.x));
  CHECK(std::isfinite(right.x));
  const float sep = std::sqrt(
      (left.x - right.x) * (left.x - right.x) + (left.y - right.y) * (left.y - right.y)
      + (left.z - right.z) * (left.z - right.z));
  CHECK(sep > 0.f);
  CHECK(left.y == Approx(right.y));
}

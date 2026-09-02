// Coverage for Threedim/Primitive.cpp (Plane, Cube, Sphere, Icosahedron, Cone,
// Cylinder, Torus + the shared createMesh/loadTriMesh expansion) and
// Threedim/ArrayToGeometry.cpp (ArrayToMesh). All of it is pure CPU: vcglib
// builds the mesh, createMesh de-indexes it into the flat
// position|normal|texcoord float buffer that PrimitiveOutputs publishes. No
// QRhi, no display.
//
// The assertions are on the *generated geometry* — face counts, bounds,
// normal lengths, the buffer partition and the texcoord convention — not on
// "it returned something".

#include "ForkProbe.hpp"

#include <Threedim/ArrayToGeometry.hpp>
#include <Threedim/Primitive.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using Catch::Approx;

namespace
{
struct Span
{
  const float* pos{};
  const float* nrm{};
  const float* uv{};
  int64_t vertices{};
};

// The buffer partition every Primitive publishes: [pos][normal][texcoord],
// each de-indexed to one entry per triangle corner.
template <typename Out>
Span checkLayout(const Out& outputs)
{
  const auto& g = outputs.geometry;
  const auto v = g.mesh.vertices;
  REQUIRE(v >= 0);
  CHECK(g.dirty_mesh);
  CHECK(g.mesh.buffers.main_buffer.dirty);
  CHECK(g.mesh.buffers.main_buffer.element_count == v * (3 + 3 + 2));
  CHECK(g.mesh.input.input0.byte_offset == 0);
  CHECK(g.mesh.input.input1.byte_offset == int(sizeof(float)) * v * 3);
  CHECK(g.mesh.input.input2.byte_offset == int(sizeof(float)) * v * 6);

  const float* base = g.mesh.buffers.main_buffer.elements;
  if(v == 0)
    return Span{nullptr, nullptr, nullptr, 0};
  REQUIRE(base != nullptr);
  return Span{base, base + v * 3, base + v * 6, v};
}

void checkUnitNormals(const Span& s)
{
  for(int64_t i = 0; i < s.vertices; i++)
  {
    const float x = s.nrm[3 * i], y = s.nrm[3 * i + 1], z = s.nrm[3 * i + 2];
    CHECK(std::sqrt(x * x + y * y + z * z) == Approx(1.f).margin(1e-4));
  }
}

// createMesh projects each corner onto the plane orthogonal to the face
// normal's dominant axis (box mapping). For a Z-facing surface like the
// Plane this reduces to uv = pos.xy, its natural parameterization, which
// is pinned here; solid faces parallel to Z get their own projection —
// see the cube UV-area case below.
void checkTexcoordsArePositionXY(const Span& s)
{
  for(int64_t i = 0; i < s.vertices; i++)
  {
    CHECK(s.uv[2 * i] == s.pos[3 * i]);
    CHECK(s.uv[2 * i + 1] == s.pos[3 * i + 1]);
  }
}

// Area of each triangle in UV space; a zero-area triangle samples a single
// texel line across its whole surface.
double uvArea(const Span& s, int64_t tri)
{
  const float* a = s.uv + 6 * tri;
  const double ux1 = a[2] - a[0], uy1 = a[3] - a[1];
  const double ux2 = a[4] - a[0], uy2 = a[5] - a[1];
  return 0.5 * std::abs(ux1 * uy2 - ux2 * uy1);
}

struct Bounds
{
  float lo[3]{
      std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max()};
  float hi[3]{
      std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
      std::numeric_limits<float>::lowest()};
};

Bounds bounds(const Span& s)
{
  Bounds b;
  for(int64_t i = 0; i < s.vertices; i++)
    for(int k = 0; k < 3; k++)
    {
      const float c = s.pos[3 * i + k];
      b.lo[k] = std::min(b.lo[k], c);
      b.hi[k] = std::max(b.hi[k], c);
    }
  return b;
}
} // namespace

TEST_CASE("Cube generates the 12 triangles of a unit box", "[threedim][primitive]")
{
  Threedim::Cube cube;
  cube.update();
  auto s = checkLayout(cube.outputs);

  // vcg::tri::Box over [0,1]^3: 12 triangles, de-indexed to 36 corners.
  REQUIRE(s.vertices == 36);

  auto b = bounds(s);
  for(int k = 0; k < 3; k++)
  {
    CHECK(b.lo[k] == Approx(0.f).margin(1e-6));
    CHECK(b.hi[k] == Approx(1.f).margin(1e-6));
  }
  checkUnitNormals(s);
}

// createMesh parameterizes per face (dominant-axis box mapping): a global
// uv = pos.xy would give the cube's x = 0 / x = 1 faces (uv varying only
// in y) and y = 0 / y = 1 faces (uv varying only in x) — 8 of its 12
// triangles — zero UV area, making them untexturable.
TEST_CASE(
    "Cube faces have nonzero UV area", "[threedim][primitive]")
{
  Threedim::Cube cube;
  cube.update();
  auto s = checkLayout(cube.outputs);
  REQUIRE(s.vertices == 36);
  int degenerate = 0;
  for(int64_t tri = 0; tri < s.vertices / 3; tri++)
    if(uvArea(s, tri) < 1e-9)
      degenerate++;
  CHECK(degenerate == 0);
}

TEST_CASE("Icosahedron generates 20 faces on a sphere", "[threedim][primitive]")
{
  Threedim::Icosahedron ico;
  ico.update();
  auto s = checkLayout(ico.outputs);
  REQUIRE(s.vertices == 60);

  // Every vertex of an icosahedron sits at the same distance from the origin.
  const auto radius = [&](int64_t i) {
    const float x = s.pos[3 * i], y = s.pos[3 * i + 1], z = s.pos[3 * i + 2];
    return std::sqrt(x * x + y * y + z * z);
  };
  const float r0 = radius(0);
  CHECK(r0 > 0.f);
  for(int64_t i = 1; i < s.vertices; i++)
    CHECK(radius(i) == Approx(r0).epsilon(1e-4));

  checkUnitNormals(s);
}

TEST_CASE("Sphere subdivides an icosahedron onto the unit sphere",
          "[threedim][primitive]")
{
  int64_t previous = 0;
  for(int subdiv : {1, 2, 3})
  {
    Threedim::Sphere sph;
    sph.inputs.subdiv.value = subdiv;
    sph.update();
    auto s = checkLayout(sph.outputs);

    // 20 icosahedron faces, quadrupled per subdivision level, 3 corners each.
    REQUIRE(s.vertices == 60 * (int64_t)std::pow(4, subdiv));
    CHECK(s.vertices > previous);
    previous = s.vertices;

    for(int64_t i = 0; i < s.vertices; i++)
    {
      const float x = s.pos[3 * i], y = s.pos[3 * i + 1], z = s.pos[3 * i + 2];
      CHECK(std::sqrt(x * x + y * y + z * z) == Approx(1.f).margin(1e-4));
    }
    checkUnitNormals(s);
  }
}

TEST_CASE("Cone honours r1 / r2 / height / subdivisions", "[threedim][primitive]")
{
  SECTION("truncated cone: both radii non-zero")
  {
    Threedim::Cone cone;
    cone.inputs.r1.value = 1.f;
    cone.inputs.r2.value = 2.f;
    cone.inputs.h.value = 4.f;
    cone.inputs.subdiv.value = 8;
    cone.update();
    auto s = checkLayout(cone.outputs);

    // 2 cap fans + 2 side triangles per segment = 4 * subdiv faces.
    REQUIRE(s.vertices == 3 * 4 * 8);

    auto b = bounds(s);
    // Axis is Y, centred on the origin.
    CHECK(b.lo[1] == Approx(-2.f).margin(1e-5));
    CHECK(b.hi[1] == Approx(2.f).margin(1e-5));
    // Widest radius is r2, in the XZ plane.
    CHECK(b.hi[0] == Approx(2.f).margin(1e-5));
    CHECK(b.lo[0] == Approx(-2.f).margin(1e-5));
    CHECK(b.hi[2] <= Approx(2.f).margin(1e-5));
  }

  SECTION("true cone: r1 == 0 collapses one end")
  {
    Threedim::Cone cone;
    cone.inputs.r1.value = 0.f;
    cone.inputs.r2.value = 3.f;
    cone.inputs.h.value = 2.f;
    cone.inputs.subdiv.value = 12;
    cone.update();
    auto s = checkLayout(cone.outputs);

    // One cap fan + one side fan = 2 * subdiv faces.
    REQUIRE(s.vertices == 3 * 2 * 12);
    auto b = bounds(s);
    CHECK(b.lo[1] == Approx(-1.f).margin(1e-5));
    CHECK(b.hi[1] == Approx(1.f).margin(1e-5));
    CHECK(b.hi[0] == Approx(3.f).margin(1e-5));
  }

  SECTION("more subdivisions produce proportionally more geometry")
  {
    Threedim::Cone a, b;
    a.inputs.r1.value = b.inputs.r1.value = 1.f;
    a.inputs.r2.value = b.inputs.r2.value = 1.f;
    a.inputs.h.value = b.inputs.h.value = 1.f;
    a.inputs.subdiv.value = 8;
    b.inputs.subdiv.value = 16;
    a.update();
    b.update();
    CHECK(b.outputs.geometry.mesh.vertices == 2 * a.outputs.geometry.mesh.vertices);
  }
}

TEST_CASE("Cylinder is a capped unit-radius tube along Y", "[threedim][primitive]")
{
  Threedim::Cylinder cyl;
  cyl.inputs.slices.value = 8;
  cyl.inputs.stacks.value = 4;
  cyl.update();
  auto s = checkLayout(cyl.outputs);

  // 2 faces per (slice, stack) quad + one cap fan of `slices` at each end.
  REQUIRE(s.vertices == 3 * (2 * 8 * 4 + 2 * 8));

  auto b = bounds(s);
  CHECK(b.lo[1] == Approx(-1.f).margin(1e-5));
  CHECK(b.hi[1] == Approx(1.f).margin(1e-5));
  for(int64_t i = 0; i < s.vertices; i++)
  {
    const float x = s.pos[3 * i], z = s.pos[3 * i + 2];
    CHECK(std::sqrt(x * x + z * z) <= Approx(1.f).margin(1e-5));
  }
}

TEST_CASE("Torus lies in the r1 +/- r2 annulus", "[threedim][primitive]")
{
  Threedim::Torus t;
  t.inputs.r1.value = 10.f;
  t.inputs.r2.value = 2.f;
  t.inputs.hdiv.value = 8;
  t.inputs.vdiv.value = 4;
  t.update();
  auto s = checkLayout(t.outputs);

  REQUIRE(s.vertices == 3 * 2 * 8 * 4);

  for(int64_t i = 0; i < s.vertices; i++)
  {
    const float x = s.pos[3 * i], y = s.pos[3 * i + 1], z = s.pos[3 * i + 2];
    const float ring = std::sqrt(x * x + y * y);
    CHECK(ring >= Approx(8.f).margin(1e-4));
    CHECK(ring <= Approx(12.f).margin(1e-4));
    CHECK(std::abs(z) <= Approx(2.f).margin(1e-4));
  }
  checkUnitNormals(s);
}

TEST_CASE("Plane is a flat unit grid with Z normals", "[threedim][primitive]")
{
  Threedim::Plane p;
  p.inputs.hdivs.value = 5;
  p.inputs.vdivs.value = 3;
  p.update();
  auto s = checkLayout(p.outputs);

  // vcg::tri::Grid(w, h): (w-1)*(h-1)*2 faces.
  REQUIRE(s.vertices == 3 * 2 * (5 - 1) * (3 - 1));

  auto b = bounds(s);
  CHECK(b.lo[0] == Approx(0.f).margin(1e-6));
  CHECK(b.hi[0] == Approx(1.f).margin(1e-6));
  CHECK(b.lo[1] == Approx(0.f).margin(1e-6));
  CHECK(b.hi[1] == Approx(1.f).margin(1e-6));
  CHECK(b.lo[2] == Approx(0.f).margin(1e-6));
  CHECK(b.hi[2] == Approx(0.f).margin(1e-6));

  for(int64_t i = 0; i < s.vertices; i++)
    CHECK(std::abs(s.nrm[3 * i + 2]) == Approx(1.f).margin(1e-5));

  checkTexcoordsArePositionXY(s);
}

TEST_CASE("Plane clamps its divisions to at least 2", "[threedim][primitive]")
{
  // The spinbox range starts at 2, but a preset / remote-control write can put
  // any int in there. vcg::tri::Grid divides by (w-1), so 1 or 0 must not
  // reach it.
  for(int bad : {-4, 0, 1})
  {
    Threedim::Plane p;
    p.inputs.hdivs.value = bad;
    p.inputs.vdivs.value = bad;
    p.update();
    auto s = checkLayout(p.outputs);
    // Clamped to the 2x2 grid: 2 triangles.
    CHECK(s.vertices == 6);
  }
}

#if defined(THREEDIM_HAS_FORK)
TEST_CASE(
    "a cone with both radii at zero must not read out of bounds",
    "[threedim][primitive]")
{
  // DEFECT (memory safety). Both R1 and R2 reach 0 from the UI sliders.
  // vcg::tri::Cone allocates SubDiv+2 vertices in that case but its two
  // ring-filling loops are guarded by `if(r1!=0)` / `if(r2!=0)`, so with both
  // radii zero it fills none of them: ivp[2..SubDiv+1] stay as the
  // uninitialised contents of `new VertexPointer[VN]`. Every generated face
  // then points at garbage, and the first thing createMesh() does is
  //   Clean<>::RemoveUnreferencedVertex -> referredVec[tri::Index(m, f.V(j))]
  // which indexes a std::vector<bool> with a wild offset. This build catches
  // it as a _GLIBCXX_ASSERTIONS abort; without that hardening it is an
  // out-of-bounds write into the heap.
  //
  // The guard belongs in Threedim::Cone::update() (or in createMesh), not in
  // vcglib.
  CHECK(threedim_test::survives([] {
    Threedim::Cone cone;
    cone.inputs.r1.value = 0.f;
    cone.inputs.r2.value = 0.f;
    cone.inputs.h.value = 1.f;
    cone.inputs.subdiv.value = 8;
    cone.update();
  }));
}
#endif

TEST_CASE(
    "a zero-height cone publishes only the faces that survived cleanup",
    "[threedim][primitive]")
{
  // Height 0 makes the top and bottom rings coincide, so all 2*subdiv side
  // triangles are zero-area and RemoveZeroAreaFace deletes them; the two cap
  // fans (2*subdiv faces) are the only real geometry left.
  //
  // vcglib deletes lazily (a flag on the face, no compaction), so
  // createMesh compacts the mesh before sizing and filling its output from
  // `mesh.face.size()` — otherwise deleted faces would still be expanded
  // into the published buffer and none of the three cleanup calls
  // (RemoveUnreferencedVertex / RemoveZeroAreaFace / RemoveNonManifoldFace)
  // could affect what the node emits.
  Threedim::Cone cone;
  cone.inputs.r1.value = 1.f;
  cone.inputs.r2.value = 1.f;
  cone.inputs.h.value = 0.f;
  cone.inputs.subdiv.value = 8;
  cone.update();

  CHECK(cone.outputs.geometry.mesh.vertices == 3 * 2 * 8);
}

TEST_CASE("ArrayToMesh ignores inputs too short for one vertex",
          "[threedim][arraytomesh]")
{
  Threedim::ArrayToMesh n;
  std::vector<float> v{1.f, 2.f};
  n.create_mesh(v);
  CHECK(n.outputs.geometry.mesh.vertices == 0);
  CHECK_FALSE(n.outputs.geometry.dirty_mesh);
  CHECK(n.outputs.geometry.mesh.buffers.main_buffer.element_count == 0);

  std::vector<float> empty;
  n.create_mesh(empty);
  CHECK_FALSE(n.outputs.geometry.dirty_mesh);
}

TEST_CASE("ArrayToMesh copies raw points into the position stream",
          "[threedim][arraytomesh]")
{
  Threedim::ArrayToMesh n;
  n.inputs.triangulate.value = false;
  std::vector<float> v{
      0.f, 0.f, 0.f, //
      1.f, 0.f, 0.f, //
      0.f, 1.f, 0.f};
  n.create_mesh(v);

  auto& g = n.outputs.geometry;
  REQUIRE(g.mesh.vertices == 3);
  CHECK(g.dirty_mesh);
  CHECK(g.mesh.buffers.main_buffer.element_count == 3 * 8);
  CHECK(g.mesh.input.input0.byte_offset == 0);
  CHECK(g.mesh.input.input1.byte_offset == int(sizeof(float)) * 9);
  CHECK(g.mesh.input.input2.byte_offset == int(sizeof(float)) * 18);

  const float* p = g.mesh.buffers.main_buffer.elements;
  REQUIRE(p != nullptr);
  for(std::size_t i = 0; i < v.size(); i++)
    CHECK(p[i] == v[i]);
  // The normal/texcoord regions are left zeroed, not filled with junk.
  for(int i = 9; i < 24; i++)
    CHECK(p[i] == 0.f);
}

TEST_CASE("ArrayToMesh truncates a partial trailing vertex",
          "[threedim][arraytomesh]")
{
  // 10 floats = 3 whole points + one stray float.
  Threedim::ArrayToMesh n;
  n.inputs.triangulate.value = false;
  std::vector<float> v(10, 1.f);
  n.create_mesh(v);
  CHECK(n.outputs.geometry.mesh.vertices == 3);
  // The buffer must still be large enough for the three streams it advertises.
  CHECK(n.outputs.geometry.mesh.buffers.main_buffer.element_count >= 3 * 8);
}

TEST_CASE("ArrayToMesh triangulation keeps the buffer self-consistent",
          "[threedim][arraytomesh]")
{
  // BallPivoting over an arbitrary cloud may or may not close a surface; what
  // must hold either way is that the published counts match the buffer.
  Threedim::ArrayToMesh n;
  n.inputs.triangulate.value = true;
  std::vector<float> v;
  for(int i = 0; i < 6; i++)
    for(int j = 0; j < 6; j++)
    {
      v.push_back(0.02f * i);
      v.push_back(0.02f * j);
      v.push_back(0.f);
    }
  n.create_mesh(v);

  auto& g = n.outputs.geometry;
  CHECK(g.mesh.vertices >= 0);
  CHECK(g.mesh.vertices % 3 == 0);
  CHECK(g.mesh.buffers.main_buffer.element_count == g.mesh.vertices * 8);
}

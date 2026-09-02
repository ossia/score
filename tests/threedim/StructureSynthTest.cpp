// StrucSynth (Structure Synth): EisenScript -> triangle mesh through the
// vendored libssynth (Preprocessor -> Tokenizer -> EisenParser -> Builder ->
// ObjRenderer) and back in through Threedim::ObjFromString.
//
// The whole unit is the synchronous seam StrucSynth::worker::work(script):
// a pure static function the host calls on a worker thread; it returns a
// closure that the processing thread applies to the object. operator()() is
// empty. So the test drives work() directly and applies the closure inline —
// no threads, no polling, no GPU, no QApplication (libssynth's ProgressDialog
// is a no-op stub in Builder.h).
//
// Geometry contract computed on paper from the shipped sources:
//  - `box` (PrimitiveRule::apply): unit cube spanning [0,1]^3, 6 quads with
//    per-face axis-aligned normals (ObjRenderer::drawBox/addQuad). tinyobj
//    triangulates each quad into 2 triangles => 36 corner-vertices.
//  - ObjFromString packs positions-then-normals (no texcoords in ssynth OBJ
//    output), so m_vertexData = 36*3 + 36*3 = 216 floats and the normals
//    stream starts at float index size/2.
//  - `N * { x T } rulename` (Action::apply): counters start at 1, so the
//    first instance is already translated once => offsets T, 2T, ... N*T.
//  - `sphere` (CreateUnitSphere, sphereDT = sphereDP = 10): 10 pole
//    triangles + 90 quads => 190 triangles => 570 corner-vertices, centered
//    on matrix*(0.5,0.5,0.5) with radius 0.5.
//  - `set seed N` resets the global RandomStreams before any rule choice, so
//    two runs of the same seeded script must be bit-identical.

#include <Threedim/StructureSynth.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <string_view>

namespace
{
using Threedim::StrucSynth;

std::function<void(StrucSynth&)> generate(std::string_view script)
{
  return StrucSynth::worker::work(script);
}

struct MeshView
{
  const float* pos{};
  const float* nrm{};
  std::size_t vertices{};
};

MeshView view(const StrucSynth& s)
{
  const auto& d = s.m_vertexData;
  return {d.data(), d.data() + d.size() / 2, d.size() / 6};
}

void bounds(const float* pos, std::size_t n, float (&mn)[3], float (&mx)[3])
{
  for(int a = 0; a < 3; a++)
  {
    mn[a] = std::numeric_limits<float>::max();
    mx[a] = std::numeric_limits<float>::lowest();
  }
  for(std::size_t i = 0; i < n; i++)
  {
    for(int a = 0; a < 3; a++)
    {
      mn[a] = std::min(mn[a], pos[3 * i + a]);
      mx[a] = std::max(mx[a], pos[3 * i + a]);
    }
  }
}
}

TEST_CASE(
    "StructureSynth: empty or blank program yields no update function",
    "[threedim][ssynth]")
{
  // work() early-returns on an empty view; a blank program parses to an empty
  // ruleset which renders nothing, and an empty OBJ also maps to no closure.
  CHECK(!generate(""));
  CHECK(!generate(" \n\t \n"));
}

TEST_CASE(
    "StructureSynth: malformed programs degrade to no update function",
    "[threedim][ssynth]")
{
  // Parser throws (caught inside CreateObj -> empty OBJ -> empty closure):
  CHECK(!generate("rule R1 { box"));  // unterminated rule body
  CHECK(!generate("box }"));          // stray brace at ruleset scope
  CHECK(!generate("3 * box"));        // loop without a transformation group
  // resolveNames throws on an unknown rule name:
  CHECK(!generate("frobnicate"));
}

TEST_CASE(
    "StructureSynth: a single box yields one triangulated unit cube",
    "[threedim][ssynth]")
{
  auto fn = generate("box");
  REQUIRE(fn);

  StrucSynth s;
  fn(s);

  // 6 quads -> 12 triangles -> 36 corner vertices; positions + normals.
  REQUIRE(s.m_vertexData.size() == 216);
  CHECK(s.outputs.geometry.mesh.vertices == 36);
  CHECK(s.outputs.geometry.mesh.buffers.main_buffer.element_count == 216);
  CHECK(s.outputs.geometry.mesh.buffers.main_buffer.elements == s.m_vertexData.data());
  CHECK(s.outputs.geometry.mesh.buffers.main_buffer.dirty);
  CHECK(s.outputs.geometry.dirty_mesh);
  // Normals stream starts right after the 36*3 position floats.
  CHECK(
      s.outputs.geometry.mesh.input.input1.byte_offset == int(108 * sizeof(float)));

  const auto v = view(s);
  REQUIRE(v.vertices == 36);

  // Every position coordinate is a cube corner coordinate: exactly 0 or 1.
  for(std::size_t i = 0; i < 3 * v.vertices; i++)
  {
    const float c = v.pos[i];
    CHECK((std::abs(c) < 1e-5f || std::abs(c - 1.f) < 1e-5f));
  }
  float mn[3], mx[3];
  bounds(v.pos, v.vertices, mn, mx);
  for(int a = 0; a < 3; a++)
  {
    CHECK(std::abs(mn[a] - 0.f) < 1e-5f);
    CHECK(std::abs(mx[a] - 1.f) < 1e-5f);
  }

  // Normals: unit, axis-aligned, and each of the 6 face directions appears on
  // exactly 6 triangulated corners (2 triangles x 3 corners per face).
  int tally[6] = {};
  for(std::size_t i = 0; i < v.vertices; i++)
  {
    const float* n = v.nrm + 3 * i;
    int axis = -1, dominant = 0;
    for(int a = 0; a < 3; a++)
    {
      if(std::abs(n[a]) > 0.5f)
      {
        axis = a;
        dominant++;
      }
    }
    REQUIRE(dominant == 1);
    CHECK(std::abs(std::abs(n[axis]) - 1.f) < 1e-4f);
    for(int a = 0; a < 3; a++)
      if(a != axis)
        CHECK(std::abs(n[a]) < 1e-4f);
    tally[2 * axis + (n[axis] < 0.f ? 1 : 0)]++;
  }
  for(int d = 0; d < 6; d++)
    CHECK(tally[d] == 6);
}

TEST_CASE(
    "StructureSynth: '3 * { x 2 } box' yields three boxes at x = 2, 4, 6",
    "[threedim][ssynth]")
{
  auto fn = generate("3 * { x 2 } box");
  REQUIRE(fn);

  StrucSynth s;
  fn(s);

  // 3 boxes x 36 corners x (3 pos + 3 nrm) floats.
  REQUIRE(s.m_vertexData.size() == 648);
  CHECK(s.outputs.geometry.mesh.vertices == 108);
  CHECK(
      s.outputs.geometry.mesh.input.input1.byte_offset == int(324 * sizeof(float)));

  const auto v = view(s);
  REQUIRE(v.vertices == 108);

  // The loop counter starts at 1 (Action::apply), so the first box is already
  // translated: boxes span x in [2,3], [4,5], [6,7]; y and z stay in [0,1].
  float mn[3], mx[3];
  bounds(v.pos, v.vertices, mn, mx);
  CHECK(std::abs(mn[0] - 2.f) < 1e-4f);
  CHECK(std::abs(mx[0] - 7.f) < 1e-4f);
  for(int a = 1; a < 3; a++)
  {
    CHECK(std::abs(mn[a] - 0.f) < 1e-4f);
    CHECK(std::abs(mx[a] - 1.f) < 1e-4f);
  }

  // Each box contributes exactly its 36 corners to its own x-band, and every
  // coordinate is still an integer (pure translation of cube corners).
  std::size_t band[3] = {};
  for(std::size_t i = 0; i < v.vertices; i++)
  {
    const float* p = v.pos + 3 * i;
    for(int a = 0; a < 3; a++)
      CHECK(std::abs(p[a] - std::round(p[a])) < 1e-4f);
    if(p[0] < 3.5f)
      band[0]++;
    else if(p[0] < 5.5f)
      band[1]++;
    else
      band[2]++;
  }
  CHECK(band[0] == 36);
  CHECK(band[1] == 36);
  CHECK(band[2] == 36);
}

TEST_CASE(
    "StructureSynth: a sphere tessellates to 570 vertices on the r=0.5 shell",
    "[threedim][ssynth]")
{
  auto fn = generate("sphere");
  REQUIRE(fn);

  StrucSynth s;
  fn(s);

  // CreateUnitSphere(10, 10): 10 pole triangles + 90 quads -> 190 triangles
  // -> 570 corners (positions + normals = 3420 floats).
  REQUIRE(s.m_vertexData.size() == 3420);
  CHECK(s.outputs.geometry.mesh.vertices == 570);

  const auto v = view(s);
  REQUIRE(v.vertices == 570);

  // PrimitiveRule::apply: center = matrix*(0.5,0.5,0.5), radius = 0.5.
  // Every vertex sits on that shell and its normal points radially outward.
  for(std::size_t i = 0; i < v.vertices; i++)
  {
    const float* p = v.pos + 3 * i;
    const float* n = v.nrm + 3 * i;
    const float dx = p[0] - 0.5f, dy = p[1] - 0.5f, dz = p[2] - 0.5f;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    REQUIRE(std::abs(dist - 0.5f) < 1e-3f);

    const float nlen = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    REQUIRE(std::abs(nlen - 1.f) < 1e-3f);
    // normal // (p - center):
    const float dot = (n[0] * dx + n[1] * dy + n[2] * dz) / (nlen * dist);
    REQUIRE(dot > 0.999f);
  }
}

TEST_CASE(
    "StructureSynth: 'set seed' makes recursive random rules deterministic",
    "[threedim][ssynth]")
{
  // Two rules named R1 form an AmbiguousRule: each expansion draws from the
  // global RandomStreams. 'set seed 17' is a top-level set action executed
  // before the first choice, so two full work() runs must be bit-identical
  // even though the second run inherits the first run's RNG state.
  static constexpr std::string_view script = R"(set seed 17
set maxdepth 12
R1
rule R1 { box { x 2 } R1 }
rule R1 { box { y 2 } R1 }
)";

  auto fn1 = generate(script);
  auto fn2 = generate(script);
  REQUIRE(fn1);
  REQUIRE(fn2);

  StrucSynth a, b;
  fn1(a);
  fn2(b);

  // At least one box, whole boxes only (36 corners x 6 floats each).
  REQUIRE(a.m_vertexData.size() >= 216);
  CHECK(a.m_vertexData.size() % 216 == 0);

  REQUIRE(a.m_vertexData.size() == b.m_vertexData.size());
  CHECK(std::equal(
      a.m_vertexData.begin(), a.m_vertexData.end(), b.m_vertexData.begin()));
}

TEST_CASE(
    "StructureSynth: a line-only program produces no phantom triangles",
    "[threedim][ssynth]")
{
  // `grid` renders only OBJ 'l' statements (ObjRenderer::addLineQuad, no
  // normals); tinyobj files those under shape.lines, so no face vertices
  // exist. Whatever comes back must not invent triangle geometry.
  auto fn = generate("grid");
  if(fn)
  {
    StrucSynth s;
    fn(s);
    CHECK(s.outputs.geometry.mesh.vertices == 0);
    CHECK(s.outputs.geometry.mesh.buffers.main_buffer.element_count == 0);
  }
  else
  {
    SUCCEED("line-only program rejected outright, which is also graceful");
  }
}

TEST_CASE(
    "StructureSynth: an on-the-fly triangle rule yields its three vertices",
    "[threedim][ssynth][!shouldfail]")
{
  // DEFECT: StrucSynth::worker::work() hard-codes the positions+normals
  // buffer layout of box/sphere output: it reports
  //   vertices    = m_vertexData.size() / (2 * 3)
  //   input1.byte_offset = sizeof(float) * (size / 2)
  // But RuleSet::resolveNames() creates TriangleRule on the fly for
  // "triangle[p1;p2;p3]" scripts, and ObjRenderer::drawTriangle emits its
  // face with nID = -1 — an OBJ with no `vn` at all — so ObjFromString
  // returns a position-only buffer (9 floats for one triangle). The closure
  // then reports 9/6 = 1 vertex instead of 3 and points the normals binding
  // at byte 16, into the middle of the position stream. Correct behaviour
  // (asserted here): the triangle's 3 vertices reach the geometry output —
  // either by synthesizing normals for normal-less meshes or by honoring
  // mesh.normals from ObjFromString.
  auto fn = generate("triangle[0,0,0;1,0,0;0,1,0]");
  REQUIRE(fn);

  StrucSynth s;
  fn(s);
  CHECK(s.outputs.geometry.mesh.vertices == 3);
}

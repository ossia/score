// Behavioural coverage for Threedim::TextToMesh (Threedim/TextToMesh.cpp):
// the QRawFont -> QPainterPath -> ear-clip tessellation pipeline and the
// scene_state it publishes. Everything here is the CPU rebuild()/operator()()
// path — init/update/release (the GpuResourceRegistry side) are never called,
// so no QRhi and no display is touched by the assertions themselves.
//
// QRawFont/QFont need the platform font database, which only exists once a
// QGuiApplication is constructed — made on first use, same shape as
// score-plugin-gfx/tests/InteropRingPolicyTest.cpp. If QT_QPA_PLATFORM is
// unset we pick "offscreen" so the test behaves identically with and without
// a display.
//
// Fonts differ per host ("Sans" resolves to whatever fontconfig says), so
// every assertion is font-INDEPENDENT: structure, finiteness, buffer/attribute
// consistency, monotonic width growth, exact linear height scaling (the
// pixel->world scale is linear in the Height control for a fixed font),
// centering algebra, winding vs. the published +Z normal, and the
// mesh-reuse / version / dirty semantics. NEVER absolute vertex counts or
// exact coordinates. Hosts with no usable scalable font SKIP the
// geometry-producing cases instead of failing.

#include <Threedim/TextToMesh.hpp>

#include <QFont>
#include <QGuiApplication>
#include <QPainterPath>
#include <QRawFont>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

using Catch::Approx;

namespace
{

// The font database requires a QGuiApplication. Catch2 owns main(), so make
// one on first use (InteropRingPolicyTest pattern). Forcing "offscreen" when
// nothing is requested keeps the run deterministic on headless CI.
void ensureApp()
{
  if(!qApp)
  {
    if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", "offscreen");
    static int argc = 1;
    static char arg0[] = "TextToMeshTest";
    static char* argv[] = {arg0, nullptr};
    // Deliberately leaked: a static Q*Application is destroyed from the atexit
    // chain, after main returns and Qt's own static state is gone, which faults
    // in ~QGuiApplication/~QCoreApplication on Windows. Same pattern as
    // tests/unit/InfiniteScrollerTest.cpp.
    static auto* app = new QGuiApplication(argc, argv);
    (void)app;
  }
}

// Mirror the node's own font resolution: if this host cannot produce a
// non-empty outline for 'H' through QRawFont, TextToMesh legitimately outputs
// an empty scene and the geometry assertions are unanswerable — SKIP, don't
// fail.
bool hostFontUsable()
{
  ensureApp();
  QFont qf(QStringLiteral("Sans"));
  qf.setPixelSize(72);
  QRawFont rf = QRawFont::fromFont(qf);
  if(!rf.isValid())
  {
    QFont def;
    def.setPixelSize(72);
    rf = QRawFont::fromFont(def);
  }
  if(!rf.isValid())
    return false;
  const auto glyphs = rf.glyphIndexesForString(QStringLiteral("H"));
  if(glyphs.isEmpty())
    return false;
  return !rf.pathForGlyph(glyphs[0]).isEmpty();
}

struct Built
{
  std::unique_ptr<Threedim::TextToMesh> node;
  ossia::scene_state_ptr state;
};

Built build(const std::string& text, float height = 1.f, bool center = false)
{
  ensureApp();
  auto n = std::make_unique<Threedim::TextToMesh>();
  n->inputs.text.value = text;
  n->inputs.height.value = height;
  n->inputs.center_x.value = center;
  n->rebuild();
  (*n)();
  auto st = n->outputs.scene_out.scene.state;
  return Built{std::move(n), st};
}

const float* floatData(const ossia::buffer_resource_ptr& b, int64_t expected_bytes)
{
  REQUIRE(b);
  auto* bd = ossia::get_if<ossia::buffer_data>(&b->resource);
  REQUIRE(bd);
  CHECK(bd->byte_size == expected_bytes);
  REQUIRE(bd->data);
  return static_cast<const float*>(bd->data.get());
}

// Validated view over the single mesh TextToMesh publishes. REQUIREs the
// structural contract from the source: one root named "Text" whose children
// are [scene_transform, mesh_component]; one triangle-list primitive with
// position/normal/texcoord0 attributes in buffers 0/1/2 and uint32 indices.
struct MeshView
{
  ossia::mesh_component_ptr mesh; // keeps buffers alive
  const ossia::mesh_primitive* prim{};
  const ossia::scene_transform* transform{};
  const float* pos{};
  const float* nrm{};
  const float* uv{};
  const uint32_t* idx{};
  std::size_t vertices{};
  std::size_t indices{};
};

MeshView inspect(const ossia::scene_state& s)
{
  MeshView v;
  REQUIRE(s.roots);
  REQUIRE(s.roots->size() == 1);
  const auto& root = (*s.roots)[0];
  REQUIRE(root);
  CHECK(root->name == "Text");
  REQUIRE(root->children);
  REQUIRE(root->children->size() == 2);

  v.transform = ossia::get_if<ossia::scene_transform>(&(*root->children)[0]);
  REQUIRE(v.transform);

  auto* mc = ossia::get_if<ossia::mesh_component_ptr>(&(*root->children)[1]);
  REQUIRE(mc);
  REQUIRE(*mc);
  v.mesh = *mc;
  REQUIRE(v.mesh->primitives.size() == 1);
  v.prim = &v.mesh->primitives[0];

  const auto& p = *v.prim;
  CHECK(p.topology == ossia::primitive_topology::triangles);
  CHECK(p.stable_id != 0);
  REQUIRE(p.vertex_count >= 3);
  REQUIRE(p.index_count >= 3);
  CHECK(p.index_count % 3 == 0);
  v.vertices = p.vertex_count;
  v.indices = p.index_count;

  REQUIRE(p.vertex_buffers.size() == 3);
  REQUIRE(p.attributes.size() == 3);
  const auto& ap = p.attributes[0];
  const auto& an = p.attributes[1];
  const auto& at = p.attributes[2];
  CHECK(ap.semantic == ossia::attribute_semantic::position);
  CHECK(ap.format == ossia::vertex_format::float3);
  CHECK(ap.buffer_index == 0);
  CHECK(ap.byte_stride == 3 * sizeof(float));
  CHECK(ap.rate == ossia::vertex_attribute::input_rate::per_vertex);
  CHECK(an.semantic == ossia::attribute_semantic::normal);
  CHECK(an.format == ossia::vertex_format::float3);
  CHECK(an.buffer_index == 1);
  CHECK(at.semantic == ossia::attribute_semantic::texcoord0);
  CHECK(at.format == ossia::vertex_format::float2);
  CHECK(at.buffer_index == 2);

  const auto v3 = int64_t(v.vertices) * 3 * sizeof(float);
  const auto v2 = int64_t(v.vertices) * 2 * sizeof(float);
  v.pos = floatData(p.vertex_buffers[0], v3);
  v.nrm = floatData(p.vertex_buffers[1], v3);
  v.uv = floatData(p.vertex_buffers[2], v2);

  REQUIRE(p.index_buffer);
  CHECK(p.index_type == ossia::index_format::uint32);
  auto* ib = ossia::get_if<ossia::buffer_data>(&p.index_buffer->resource);
  REQUIRE(ib);
  CHECK(ib->byte_size == int64_t(v.indices) * int64_t(sizeof(uint32_t)));
  REQUIRE(ib->data);
  v.idx = static_cast<const uint32_t*>(ib->data.get());
  return v;
}

struct Box
{
  float lo[3];
  float hi[3];
  float width() const { return hi[0] - lo[0]; }
  float height() const { return hi[1] - lo[1]; }
};

Box bbox(const MeshView& v)
{
  Box b{{v.pos[0], v.pos[1], v.pos[2]}, {v.pos[0], v.pos[1], v.pos[2]}};
  for(std::size_t i = 1; i < v.vertices; ++i)
    for(int k = 0; k < 3; ++k)
    {
      const float c = v.pos[3 * i + k];
      b.lo[k] = std::min(b.lo[k], c);
      b.hi[k] = std::max(b.hi[k], c);
    }
  return b;
}

// Doubled signed area of triangle t in the XY plane. Positive = CCW.
float triArea2(const MeshView& v, std::size_t t)
{
  const uint32_t i0 = v.idx[3 * t], i1 = v.idx[3 * t + 1], i2 = v.idx[3 * t + 2];
  const float ax = v.pos[3 * i0], ay = v.pos[3 * i0 + 1];
  const float bx = v.pos[3 * i1], by = v.pos[3 * i1 + 1];
  const float cx = v.pos[3 * i2], cy = v.pos[3 * i2 + 1];
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

} // namespace

TEST_CASE(
    "TextToMesh publishes a well-formed flat triangle mesh for non-empty text",
    "[threedim][text_to_mesh]")
{
  if(!hostFontUsable())
    SKIP("no usable scalable font on this host");

  auto b = build("Hello");
  REQUIRE(b.state);
  REQUIRE_FALSE(b.state->empty());
  CHECK(b.node->outputs.scene_out.dirty != 0);
  CHECK(b.state->version > 0);

  auto v = inspect(*b.state);

  // Identity transform on a fresh node (position 0, rotation 0, scale 1).
  CHECK(v.transform->translation[0] == 0.f);
  CHECK(v.transform->translation[1] == 0.f);
  CHECK(v.transform->translation[2] == 0.f);
  CHECK(v.transform->rotation[3] == Approx(1.f));
  CHECK(v.transform->scale[0] == 1.f);

  // Every index addresses a real vertex, and no triangle is a degenerate
  // repeat of one index (the ear-clipper always emits three distinct corners).
  for(std::size_t i = 0; i < v.indices; ++i)
    REQUIRE(v.idx[i] < v.vertices);
  for(std::size_t t = 0; t < v.indices / 3; ++t)
  {
    CHECK(v.idx[3 * t] != v.idx[3 * t + 1]);
    CHECK(v.idx[3 * t + 1] != v.idx[3 * t + 2]);
    CHECK(v.idx[3 * t] != v.idx[3 * t + 2]);
  }

  // Positions: finite, and flat in the XY plane (z == 0 for every vertex).
  for(std::size_t i = 0; i < v.vertices; ++i)
  {
    REQUIRE(std::isfinite(v.pos[3 * i]));
    REQUIRE(std::isfinite(v.pos[3 * i + 1]));
    CHECK(v.pos[3 * i + 2] == 0.f);
  }

  // Normals: exactly +Z everywhere, matching the flat XY mesh.
  for(std::size_t i = 0; i < v.vertices; ++i)
  {
    CHECK(v.nrm[3 * i] == 0.f);
    CHECK(v.nrm[3 * i + 1] == 0.f);
    CHECK(v.nrm[3 * i + 2] == 1.f);
  }

  // Texcoords are published (zero-filled in v1) and finite.
  for(std::size_t i = 0; i < 2 * v.vertices; ++i)
    REQUIRE(std::isfinite(v.uv[i]));

  // The primitive's local AABB is exactly the min/max of its positions.
  const Box box = bbox(v);
  REQUIRE_FALSE(v.prim->bounds.empty());
  for(int k = 0; k < 3; ++k)
  {
    CHECK(v.prim->bounds.min[k] == Approx(box.lo[k]));
    CHECK(v.prim->bounds.max[k] == Approx(box.hi[k]));
  }
  CHECK(box.width() > 0.f);
  CHECK(box.height() > 0.f);

  // One default white material for downstream PBR.
  REQUIRE(b.state->materials);
  REQUIRE(b.state->materials->size() == 1);
  const auto& mat = (*b.state->materials)[0];
  REQUIRE(mat);
  for(int k = 0; k < 4; ++k)
    CHECK(mat->base_color_factor[k] == 1.f);
}

TEST_CASE(
    "Empty and whitespace-only text yield an empty scene without crashing, "
    "and a later text edit recovers",
    "[threedim][text_to_mesh]")
{
  ensureApp();

  for(const char* txt : {"", " "})
  {
    auto b = build(txt);
    REQUIRE(b.state);
    CHECK(b.state->empty());
    CHECK(b.state->materials == nullptr);
    CHECK(b.node->outputs.scene_out.dirty != 0);
    // Republish with no edits: same state, no dirty.
    (*b.node)();
    CHECK(b.node->outputs.scene_out.scene.state == b.state);
    CHECK(b.node->outputs.scene_out.dirty == 0);
  }

  if(hostFontUsable())
  {
    auto b = build("");
    CHECK(b.state->empty());
    b.node->inputs.text.value = "H";
    b.node->rebuild();
    (*b.node)();
    REQUIRE(b.node->outputs.scene_out.scene.state);
    CHECK_FALSE(b.node->outputs.scene_out.scene.state->empty());
    CHECK(b.node->outputs.scene_out.dirty != 0);
  }
}

TEST_CASE(
    "Lazy first call: operator() on a fresh node builds the default text",
    "[threedim][text_to_mesh]")
{
  if(!hostFontUsable())
    SKIP("no usable scalable font on this host");

  ensureApp();
  Threedim::TextToMesh n; // default text is "Hello"
  n();
  REQUIRE(n.outputs.scene_out.scene.state);
  CHECK_FALSE(n.outputs.scene_out.scene.state->empty());
  CHECK(n.outputs.scene_out.dirty != 0);
}

TEST_CASE(
    "A longer string is wider than a single glyph, at the same glyph height",
    "[threedim][text_to_mesh]")
{
  if(!hostFontUsable())
    SKIP("no usable scalable font on this host");

  auto one = build("M");
  auto three = build("MMM");
  REQUIRE_FALSE(one.state->empty());
  REQUIRE_FALSE(three.state->empty());
  const Box b1 = bbox(inspect(*one.state));
  const Box b3 = bbox(inspect(*three.state));

  // The pen advance accumulates per glyph, so more glyphs = strictly wider.
  CHECK(b3.width() > b1.width());
  // Same glyph, same font, same scale: identical vertical ink extent.
  CHECK(b3.height() == Approx(b1.height()).margin(1e-5));
  CHECK(b3.lo[1] == Approx(b1.lo[1]).margin(1e-5));
  CHECK(b3.hi[1] == Approx(b1.hi[1]).margin(1e-5));
}

TEST_CASE(
    "The Height control scales the mesh linearly",
    "[threedim][text_to_mesh]")
{
  if(!hostFontUsable())
    SKIP("no usable scalable font on this host");

  auto h1 = build("H", 1.f);
  auto h2 = build("H", 2.f);
  const Box b1 = bbox(inspect(*h1.state));
  const Box b2 = bbox(inspect(*h2.state));

  // pixel_to_world = height / (pixelSize * 0.7 + eps): linear in height for
  // a fixed font, so doubling Height exactly doubles every extent — a
  // font-independent ratio even though the absolute size is not.
  REQUIRE(b1.width() > 0.f);
  REQUIRE(b1.height() > 0.f);
  CHECK(b2.width() / b1.width() == Approx(2.f).epsilon(1e-4));
  CHECK(b2.height() / b1.height() == Approx(2.f).epsilon(1e-4));
}

TEST_CASE(
    "Center X preserves the ink width and moves the text to straddle the origin",
    "[threedim][text_to_mesh]")
{
  if(!hostFontUsable())
    SKIP("no usable scalable font on this host");

  auto plain = build("MM", 1.f, false);
  auto centered = build("MM", 1.f, true);
  const Box bp = bbox(inspect(*plain.state));
  const Box bc = bbox(inspect(*centered.state));

  // Centering only subtracts half the total advance from X: the ink width
  // and the Y extent are untouched.
  CHECK(bc.width() == Approx(bp.width()).margin(1e-5));
  CHECK(bc.lo[1] == Approx(bp.lo[1]).margin(1e-5));
  CHECK(bc.hi[1] == Approx(bp.hi[1]).margin(1e-5));
  // The shift is leftward (total advance is positive)...
  CHECK(bc.lo[0] < bp.lo[0]);
  // ...and any two-glyph string's ink straddles the origin once centered:
  // ink starts left of one advance and ends right of it.
  CHECK(bc.lo[0] < 0.f);
  CHECK(bc.hi[0] > 0.f);
}

TEST_CASE(
    "Triangles wind counter-clockwise in XY, consistent with the +Z normal",
    "[threedim][text_to_mesh]")
{
  if(!hostFontUsable())
    SKIP("no usable scalable font on this host");

  // "Hello" includes glyphs whose fill polygons come out of Qt in both
  // windings (outer contours and 'e'/'o' counters); earClip() re-orients
  // every polygon to CCW before clipping, so every emitted triangle must
  // have non-negative signed area in the Y-up frame the +Z normal claims.
  auto b = build("Hello");
  auto v = inspect(*b.state);
  const Box box = bbox(v);
  const float ext = std::max(box.width(), box.height());
  // Tolerance for the final leftover triangle of a near-degenerate contour;
  // scale-relative so the test is stable across fonts.
  const float eps = 1e-4f * ext * ext;

  float total = 0.f;
  std::size_t negative = 0;
  for(std::size_t t = 0; t < v.indices / 3; ++t)
  {
    const float a2 = triArea2(v, t);
    total += a2;
    if(a2 < -eps)
      ++negative;
  }
  CHECK(negative == 0);
  CHECK(total > 0.f);
}

TEST_CASE(
    "TRS-only edits reuse the cached mesh and bump the version; text edits "
    "retessellate",
    "[threedim][text_to_mesh]")
{
  if(!hostFontUsable())
    SKIP("no usable scalable font on this host");

  auto b = build("Hi");
  REQUIRE(b.state);
  auto v0 = inspect(*b.state);
  const auto mesh0 = v0.mesh;
  const auto version0 = b.state->version;
  CHECK(b.node->outputs.scene_out.dirty != 0);

  // Republish with no edits: same state object, version untouched, dirty
  // consumed.
  (*b.node)();
  CHECK(b.node->outputs.scene_out.scene.state == b.state);
  CHECK(b.state->version == version0);
  CHECK(b.node->outputs.scene_out.dirty == 0);

  // Rebuild with unchanged inputs: the tessellated mesh_component is reused.
  b.node->rebuild();
  (*b.node)();
  CHECK(inspect(*b.state).mesh == mesh0);

  // TRS-only edit (what the position control's update() triggers): version
  // bumps, dirty is republished, the transform child carries the new value —
  // but the expensive tessellation is NOT redone: same mesh_component.
  const auto version1 = b.state->version;
  b.node->inputs.position.value.x = 1.5f;
  b.node->rebuild();
  (*b.node)();
  CHECK(b.state->version > version1);
  CHECK(b.node->outputs.scene_out.dirty != 0);
  auto v1 = inspect(*b.state);
  CHECK(v1.mesh == mesh0);
  CHECK(v1.transform->translation[0] == Approx(1.5f));

  // Text edit: a new mesh_component is tessellated, version moves on.
  const auto version2 = b.state->version;
  b.node->inputs.text.value = "Hi!";
  b.node->rebuild();
  (*b.node)();
  auto v2 = inspect(*b.state);
  CHECK(v2.mesh != mesh0);
  CHECK(b.state->version > version2);
  CHECK(b.node->outputs.scene_out.dirty != 0);
}

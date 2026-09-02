// =============================================================================
// L3 — CSF GEOMETRY / STORAGE-BUFFER readback through BufferSinkNode.
//
// Un-SKIPs the gap documented by csf_geometry_readback_skip_reason() in
// Gfx.hpp: a compute node whose only outputs are Types::Geometry /
// Types::Buffer is now reachable from an OutputNode sink
// (score::test::gfx::BufferSinkNode), so its compute passes dispatch with NO
// raster consumer, and the SSBOs it wrote are read back byte-exactly
// (QRhiResourceUpdateBatch::readBackBuffer completing at endOffscreenFrame).
//
// Spec coverage:
//   * P1-9  — a CSF geometry producer's position/color SSBOs read back with
//             the exact float values the shader wrote (closed form below).
//   * P0-2  — the SSBO-counter half: an `instanceHits` storage buffer written
//             by a PER_INSTANCE pass reads back exactly one hit per instance,
//             proving the dispatch was sized from INSTANCE_COUNT.
//
// Closed forms, derived from the corpus shader sources:
//
//   syn-geo-producer.cs (VERTEX_COUNT 3; main() lines ~29-35):
//     geo_position_out[0] = (-1,-1,0,1)   // p = vec2(-1,-1)
//     geo_position_out[1] = ( 3,-1,0,1)   // idx==1 -> p = vec2( 3,-1)
//     geo_position_out[2] = (-1, 3,0,1)   // idx==2 -> p = vec2(-1, 3)
//     geo_color_out[i]    = ( 0, 1,0,1)   // solid green, all i
//     All literals are exactly representable floats -> compare with ==.
//
//   syn-geo-add-attribute.cs (chained after the producer; main() lines 30-39):
//     out_position[i] = in_position[i]    // pass-through of the 3 verts
//     out_color[i]    = ( 0, 0,1,1)       // blue, existing nowhere upstream
//     VERTEX_COUNT "$VERTEX_COUNT_geoIn" -> still 3 vertices.
//
//   syn-instancing.cs (VERTEX_COUNT 3, INSTANCE_COUNT 4; PASSES pass 1 is
//   PER_INSTANCE with LOCAL_SIZE 4; main() lines 47-55):
//     tally.instanceHits[idx] = 1 for each dispatched instance invocation;
//     storage buffers are zero-initialized on creation (RenderedCSFNode
//     "Initialize buffer with zero data for predictable behavior"), so a
//     correctly-sized dispatch reads back int[32] = {1,1,1,1,0,...,0} —
//     sum exactly 4. An over-dispatch reads more 1s, a missing dispatch
//     reads 0s: both fail the exact checks.
//     geo_translation_out[idx] = (idx,0,0,0) -> the per-instance vec4[4]
//     attribute reads back (0,0,0,0),(1,0,0,0),(2,0,0,0),(3,0,0,0).
//
// House rule (tests/integration/WiredCases.hpp): an empty readback on a
// backend that claims support is a FAILURE — render_csf_buffer_readback
// reports it through `error`, and these tests REQUIRE(error.empty()).
// SKIPs happen only for unavailable backends / missing compute / missing
// ReadBackNonUniformBuffer, with the reason in the message.
// =============================================================================

#include <score_test/Gfx.hpp>
#include <score_test/GfxBufferSink.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/Uniforms.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdio>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

CsfBufferResult run_readback(
    score::gfx::GraphicsApi be, std::vector<const char*> chain, int frames = 3)
{
  std::vector<QString> paths;
  for(auto* c : chain)
    paths.push_back(corpus(c));
  CsfBufferResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_buffer_readback(be, paths, {64, 64}, frames);
  });
  return r;
}

void dump(const char* tag, const CsfBufferResult& r)
{
  if(!qEnvironmentVariableIsSet("GFX_DUMP"))
    return;
  std::fprintf(
      stderr, "[%s] backend=%s skipped=%d err='%s' verts=%d inst=%d\n", tag,
      r.backend.c_str(), int(r.skipped), r.error.c_str(), r.vertices,
      r.instances);
  const auto show = [](const char* kind, const std::vector<NamedBytes>& v) {
    for(const auto& b : v)
      std::fprintf(
          stderr, "  %s '%s': %d bytes\n", kind, b.name.c_str(),
          int(b.bytes.size()));
  };
  show("attr", r.attributes);
  show("aux ", r.auxiliaries);
  show("stor", r.storage);
  std::fflush(stderr);
}

// Exact vec4 check on a float array at element index `v` (floats 4v..4v+3).
// The shader writes exactly-representable literals, so == is the right
// comparison — any conversion / stride / offset bug shows as a hard mismatch.
bool vec4_is(const std::vector<float>& f, int v, float x, float y, float z, float w)
{
  const std::size_t i = std::size_t(v) * 4;
  if(f.size() < i + 4)
    return false;
  return f[i] == x && f[i + 1] == y && f[i + 2] == z && f[i + 3] == w;
}
}

// A non-skipped anchor so Catch2 reports a run rather than exit-4 "no tests
// ran" when every rendering case below SKIPs (e.g. a box with no GPU).
// Also pins the sink's port surface: one Geometry-typed input that carries
// both geometry and storage-buffer edges (see GfxBufferSink.hpp for why a
// single port).
TEST_CASE("BufferSinkNode port surface", "[gfx][l3][csf][buffersink]")
{
  bool one_input = false;
  bool geometry_typed = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    BufferSinkNode sink;
    one_input = sink.input.size() == 1;
    geometry_typed = !sink.input.empty()
                     && sink.input[0]->type == score::gfx::Types::Geometry;
  });
  CHECK(one_input);
  CHECK(geometry_typed);
}

// P1-9: a geometry-ONLY CSF — no raster consumer, no image output — must
// dispatch and its attribute SSBOs must read back the exact values written.
// This is precisely the topology the old fixture could not drive.
TEST_CASE(
    "csf geometry-only producer: dispatch + exact SSBO readback",
    "[gfx][l3][csf][geometry][readback]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const auto r = run_readback(be, {"syn-geo-producer.cs"});
  dump("geo-producer", r);

  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());

  // The dispatch actually happened and pushed a mesh.
  REQUIRE(r.geometry_seen);
  CHECK(r.vertices == 3);
  CHECK(r.instances == 1);

  const auto* pos = r.attribute("position");
  // The shader declares SEMANTIC "color"; the engine publishes that mesh
  // attribute under ossia::geometry_attribute_semantic's display name
  // "color0" (glTF COLOR_0 -- geometry_port.hpp:43; RenderedCSFNode.cpp
  // notes the upstream mesh publishes "color0").
  const auto* col = r.attribute("color0");
  REQUIRE(pos);
  REQUIRE(col);

  // 3 vertices x vec4 at std430 stride 16 => at least 48 bytes each.
  REQUIRE(pos->bytes.size() >= qsizetype(3 * 4 * sizeof(float)));
  REQUIRE(col->bytes.size() >= qsizetype(3 * 4 * sizeof(float)));

  const auto p = as_floats(pos->bytes);
  const auto c = as_floats(col->bytes);

  // Closed form from syn-geo-producer.cs main().
  CHECK(vec4_is(p, 0, -1.f, -1.f, 0.f, 1.f));
  CHECK(vec4_is(p, 1, 3.f, -1.f, 0.f, 1.f));
  CHECK(vec4_is(p, 2, -1.f, 3.f, 0.f, 1.f));
  CHECK(vec4_is(c, 0, 0.f, 1.f, 0.f, 1.f));
  CHECK(vec4_is(c, 1, 0.f, 1.f, 0.f, 1.f));
  CHECK(vec4_is(c, 2, 0.f, 1.f, 0.f, 1.f));
}

// CSF -> CSF geometry chain, still with no raster consumer: the add-attribute
// filter reads the producer's positions and adds a colour that exists nowhere
// upstream, so a pass-through (or a missed second dispatch) cannot fake it.
// Also exercises the "$VERTEX_COUNT_geoIn" expression through the chain.
TEST_CASE(
    "csf geometry chain: producer -> add-attribute -> sink",
    "[gfx][l3][csf][geometry][readback]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const auto r
      = run_readback(be, {"syn-geo-producer.cs", "syn-geo-add-attribute.cs"});
  dump("geo-chain", r);

  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());

  REQUIRE(r.geometry_seen);
  CHECK(r.vertices == 3);

  const auto* pos = r.attribute("position");
  // The shader declares SEMANTIC "color"; the engine publishes that mesh
  // attribute under ossia::geometry_attribute_semantic's display name
  // "color0" (glTF COLOR_0 -- geometry_port.hpp:43; RenderedCSFNode.cpp
  // notes the upstream mesh publishes "color0").
  const auto* col = r.attribute("color0");
  REQUIRE(pos);
  REQUIRE(col);
  REQUIRE(pos->bytes.size() >= qsizetype(3 * 4 * sizeof(float)));
  REQUIRE(col->bytes.size() >= qsizetype(3 * 4 * sizeof(float)));

  const auto p = as_floats(pos->bytes);
  const auto c = as_floats(col->bytes);

  // Positions passed through unchanged; colour computed by the second stage.
  CHECK(vec4_is(p, 0, -1.f, -1.f, 0.f, 1.f));
  CHECK(vec4_is(p, 1, 3.f, -1.f, 0.f, 1.f));
  CHECK(vec4_is(p, 2, -1.f, 3.f, 0.f, 1.f));
  CHECK(vec4_is(c, 0, 0.f, 0.f, 1.f, 1.f));
  CHECK(vec4_is(c, 1, 0.f, 0.f, 1.f, 1.f));
  CHECK(vec4_is(c, 2, 0.f, 0.f, 1.f, 1.f));
}

// P0-2 (SSBO-counter half): the instanceHits storage buffer written by
// syn-instancing.cs's PER_INSTANCE pass reads back exactly one hit per
// instance. The buffer is reachable BOTH as a Types::Buffer output
// ("storage:N") and as an auxiliary riding the geometry ("tally") — assert
// both and that they agree, per the spec's "read MULTIPLE outputs"
// requirement.
TEST_CASE(
    "csf PER_INSTANCE pass: instanceHits storage-buffer readback",
    "[gfx][l3][csf][instancing][readback]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const auto r = run_readback(be, {"syn-instancing.cs"});
  dump("instancing", r);

  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());

  REQUIRE(r.geometry_seen);
  CHECK(r.vertices == 3);
  CHECK(r.instances == 4);

  // --- The Buffer-output path (RenderedCSFNode::bufferForOutput).
  REQUIRE(r.storage.size() >= 1);
  const QByteArray& tallyBytes = r.storage[0].bytes;
  REQUIRE(tallyBytes.size() >= qsizetype(32 * sizeof(int32_t)));

  const auto hits = as_ints(tallyBytes);
  int sum = 0;
  for(int i = 0; i < 32; ++i)
  {
    INFO("instanceHits[" << i << "] = " << hits[i]);
    if(i < 4)
      CHECK(hits[i] == 1); // one hit per instance
    else
      CHECK(hits[i] == 0); // zero-initialized, never written
    sum += hits[i];
  }
  // The P0-2 core assertion: dispatch sized from INSTANCE_COUNT (4), not the
  // vertex count (3) and not the array length (32).
  CHECK(sum == 4);

  // --- The auxiliary-on-geometry path (pushOutputGeometry attaches the
  // standalone storage buffers by RESOURCES NAME).
  const auto* aux = r.auxiliary("tally");
  REQUIRE(aux);
  REQUIRE(aux->bytes.size() >= qsizetype(32 * sizeof(int32_t)));
  CHECK(aux->bytes.left(qsizetype(32 * sizeof(int32_t)))
        == tallyBytes.left(qsizetype(32 * sizeof(int32_t))));

  // --- The per-instance attribute: translation[i] = (i,0,0,0).
  const auto* tr = r.attribute("translation");
  REQUIRE(tr);
  REQUIRE(tr->bytes.size() >= qsizetype(4 * 4 * sizeof(float)));
  const auto t = as_floats(tr->bytes);
  CHECK(vec4_is(t, 0, 0.f, 0.f, 0.f, 0.f));
  CHECK(vec4_is(t, 1, 1.f, 0.f, 0.f, 0.f));
  CHECK(vec4_is(t, 2, 2.f, 0.f, 0.f, 0.f));
  CHECK(vec4_is(t, 3, 3.f, 0.f, 0.f, 0.f));
}

// =============================================================================
// L3 -- the three scene-filter nodes: FlattenedSceneFilterNode,
// MergeGeometriesNode, SceneFilterNode. All three are user-facing processes;
// tests/nodes/Processes.cpp round-trips their models, and nothing exercised the
// filtering or the merging.
//
// The chain here is a CSF geometry producer rather than a Threedim cube through
// ScenePreprocessorNode: the predicate reads geometry_spec metadata, which any
// geometry producer supplies, so this needs neither score_plugin_threedim nor a
// document. A CSF producer emits geometry with filter_tag == 0 and
// filter_material_index == 0, the same untagged values the preprocessor stamps,
// which makes the predicate decisive: "equals 0" draws the points, "differs from
// 0" leaves nothing.
//
// SCOPE. SceneFilterNode takes a scene_spec, and the only scene_spec sources in
// the tree are halp producers reached through the Crousti wrapper; its
// tree-rewriting visitor also lives in an anonymous namespace and is unreachable
// from another translation unit. What is asserted for it is its port surface and
// control routing; the share-copy identity contract stays uncovered.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Gfx/Graph/SceneFilterNode.hpp>

#include <ossia/detail/hash.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

int drawn_pixels(const ReadbackImage& img, int thresh = 12)
{
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) + int(p[1]) + int(p[2]) > thresh)
        ++n;
    }
  return n;
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  bool valid = false;
  int drawn = 0;
};

//! CSF geometry producer -> FlattenedSceneFilterNode(mode, match, match_str)
//! -> raw-raster -> offscreen sink.
Outcome run_filtered(
    score::gfx::GraphicsApi api, int mode, int match, const char* match_str)
{
  Outcome out;
  out.backend = backend_name(api);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addIsf(corpus("csf-vertex-count-expr.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    if(prod < 0 || raster < 0)
    {
      out.error = p.error();
      return;
    }

    auto filterNode = std::make_unique<score::gfx::FlattenedSceneFilterNode>();
    filterNode->process(1, ossia::value{mode});
    filterNode->process(2, ossia::value{match});
    filterNode->process(3, ossia::value{std::string(match_str)});
    const int filt = p.addNode(std::move(filterNode));

    p.wire(p.geometryOut(prod, 0), p.nodeGeometryIn(filt, 0));
    p.wire(p.nodeGeometryOut(filt, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    p.render(3);

    const auto img = p.readback(sink);
    out.valid = img.valid();
    if(out.valid)
      out.drawn = drawn_pixels(img);
  });
  return out;
}

//! CSF geometry producer -> MergeGeometriesNode on `port` -> raw-raster -> sink.
Outcome run_merged(score::gfx::GraphicsApi api, int port)
{
  Outcome out;
  out.backend = backend_name(api);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addIsf(corpus("csf-vertex-count-expr.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    if(prod < 0 || raster < 0)
    {
      out.error = p.error();
      return;
    }

    const int merge = p.addNode(std::make_unique<score::gfx::MergeGeometriesNode>());
    p.wire(p.geometryOut(prod, 0), p.nodeGeometryIn(merge, port));
    p.wire(p.nodeGeometryOut(merge, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    p.render(3);

    const auto img = p.readback(sink);
    out.valid = img.valid();
    if(out.valid)
      out.drawn = drawn_pixels(img);
  });
  return out;
}

void requireRan(const Outcome& o)
{
  if(o.skipped)
    SKIP(o.backend << ": " << o.skip_reason);
  REQUIRE(o.error.empty());
  REQUIRE(o.valid);
}
}

TEST_CASE(
    "FlattenedSceneFilterNode: the tag predicate decides whether a mesh survives",
    "[gfx][l3][scenefilter]")
{
  const auto be = GENERATE(from_range(platform_backends()));

  const auto kept = run_filtered(be, 0, 0, "");
  requireRan(kept);
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(why);
  CHECK(kept.drawn > 0);

  const auto dropped = run_filtered(be, 1, 0, "");
  requireRan(dropped);
  CHECK(dropped.drawn == 0);

  // Same pair on filter_material_index, which the producer also leaves at 0.
  const auto keptMat = run_filtered(be, 2, 0, "");
  requireRan(keptMat);
  CHECK(keptMat.drawn > 0);

  const auto droppedMat = run_filtered(be, 3, 0, "");
  requireRan(droppedMat);
  CHECK(droppedMat.drawn == 0);

  // A match value nothing carries drops everything under "equals" and keeps
  // everything under "differs" — the mirror image of the two above, so a
  // predicate stuck on `true` or `false` cannot pass both pairs.
  const auto noMatch = run_filtered(be, 0, 7, "");
  requireRan(noMatch);
  CHECK(noMatch.drawn == 0);

  const auto allButNoMatch = run_filtered(be, 1, 7, "");
  requireRan(allButNoMatch);
  CHECK(allButNoMatch.drawn > 0);
}

TEST_CASE(
    "FlattenedSceneFilterNode: an empty format_id matches the untagged sentinel",
    "[gfx][l3][scenefilter]")
{
  // rapidhash of the empty string is not 0, so hashing it would match nothing.
  // The implementation short-circuits to 0u instead, which is exactly the
  // filter_tag an untagged producer emits.
  CHECK((uint32_t)ossia::hash_string(std::string{}) != 0u);

  const auto be = GENERATE(from_range(platform_backends()));

  const auto untagged = run_filtered(be, 12, 0, "");
  requireRan(untagged);
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(why);
  CHECK(untagged.drawn > 0);

  const auto named = run_filtered(be, 12, 0, "nonexistent-format");
  requireRan(named);
  CHECK(named.drawn == 0);

  const auto inverted = run_filtered(be, 13, 0, "nonexistent-format");
  requireRan(inverted);
  CHECK(inverted.drawn > 0);
}

TEST_CASE(
    "FlattenedSceneFilterNode: an unknown mode keeps everything",
    "[gfx][l3][scenefilter]")
{
  const auto be = GENERATE(from_range(platform_backends()));

  const auto out = run_filtered(be, 99, 0, "");
  requireRan(out);
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(why);
  CHECK(out.drawn > 0);
}

TEST_CASE(
    "MergeGeometriesNode: a producer on a non-zero port still reaches the output",
    "[gfx][l3][scenefilter]")
{
  const auto be = GENERATE(from_range(platform_backends()));

  const auto onZero = run_merged(be, 0);
  requireRan(onZero);
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(why);
  CHECK(onZero.drawn > 0);

  // findFirstByPort keys on (port, source); an implementation indexing a dense
  // vector by port would render nothing here.
  const auto onThree = run_merged(be, 3);
  requireRan(onThree);
  CHECK(onThree.drawn > 0);
  CHECK(onThree.drawn == onZero.drawn);
}

TEST_CASE("the scene-filter nodes' port surface", "[gfx][scenefilter]")
{
  SECTION("FlattenedSceneFilterNode")
  {
    score::gfx::FlattenedSceneFilterNode n;
    REQUIRE(n.input.size() == 4);
    CHECK(n.input[0]->type == score::gfx::Types::Geometry);
    CHECK(n.input[1]->type == score::gfx::Types::Int);
    CHECK(n.input[2]->type == score::gfx::Types::Int);
    CHECK(n.input[3]->type == score::gfx::Types::Empty);
    REQUIRE(n.output.size() == 1);
    CHECK(n.output[0]->type == score::gfx::Types::Geometry);

    CHECK(n.m_mode == 0);
    CHECK(n.m_match == 0);
    CHECK(n.m_match_str.empty());

    n.process(1, ossia::value{12});
    n.process(2, ossia::value{5});
    n.process(3, ossia::value{std::string{"splat"}});
    CHECK(n.m_mode == 12);
    CHECK(n.m_match == 5);
    CHECK(n.m_match_str == "splat");
  }

  SECTION("MergeGeometriesNode exposes kMaxInputs geometry ports")
  {
    score::gfx::MergeGeometriesNode n;
    REQUIRE(n.input.size() == score::gfx::MergeGeometriesNode::kMaxInputs);
    for(auto* p : n.input)
      CHECK(p->type == score::gfx::Types::Geometry);
    REQUIRE(n.output.size() == 1);
    CHECK(n.output[0]->type == score::gfx::Types::Geometry);
  }

  SECTION("SceneFilterNode")
  {
    score::gfx::SceneFilterNode n;
    REQUIRE(n.input.size() >= 2);
    CHECK(n.input[0]->type == score::gfx::Types::Scene);
    CHECK(n.input[1]->type == score::gfx::Types::Int);
    REQUIRE(n.output.size() == 1);
    CHECK(n.output[0]->type == score::gfx::Types::Scene);

    CHECK(n.m_mode == 0);
    n.process(1, ossia::value{1});
    CHECK(n.m_mode == 1);
  }
}

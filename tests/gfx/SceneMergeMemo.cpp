// =============================================================================
// P1-4 -- N producers into one Scene In merge once and memoize.
//
// ScenePreprocessor exposes exactly one scene inlet; every upstream producer
// (glTF loader, Camera, Light, EnvironmentLoader, ...) delivers into the same
// sink port, distinguished by an opaque per-edge `source_key`
// (NodeRenderer.hpp:133-141). NodeRenderer::process(port, scene_spec, key)
// stores each delivery in m_portScenes (NodeRenderer.cpp:594) and calls
// rebuildMergedScene() (NodeRenderer.cpp:596), which:
//
//   * collects every stored scene with a non-null state -- explicitly
//     KEEPING env-only producers whose roots vector is empty
//     (NodeRenderer.cpp:488-498);
//   * memoizes on the signature {scene_state*, version} per contributor
//     (NodeRenderer.cpp:495, MergeCacheKey at NodeRenderer.hpp:216). On a
//     signature match it re-publishes the previous merged scene_spec
//     verbatim and returns without merging (NodeRenderer.cpp:500-503);
//   * short-circuits a single contributor to a plain pointer copy
//     (NodeRenderer.cpp:513-518);
//   * otherwise calls ossia::merge_scenes (NodeRenderer.cpp:524-526).
//
// COUNTING MERGES WITHOUT INSTRUMENTATION. The product code is not modified,
// so merge invocations are counted through an observable invariant instead of
// a stub: ossia::merge_scenes with >= 2 contributing states ALWAYS allocates
// a brand-new scene_state (std::make_shared<scene_state>() at
// geometry_port.cpp:474) and stamps it version = max(inputs)+1
// (geometry_port.cpp:558). The memo-hit branch instead re-assigns the cached
// scene_spec, i.e. the very same shared_ptr (NodeRenderer.cpp:502). The test
// keeps every previously observed merged scene_state alive via shared_ptr
// copies, so a freshly allocated state can never reuse an old address:
//
//     r.scene.state unchanged across a delivery  <=>  merge_scenes ran 0 times
//     r.scene.state changed                      <=>  merge_scenes ran (once
//                                                     per rebuild call)
//
// (The merged *version* cannot distinguish the two: an idle re-merge of the
// same inputs would recompute the same max+1. Pointer identity is the
// discriminating observable, hence the liveness guard above.)
//
// The "producer bumps" mirror what real halp producers do per the comment at
// NodeRenderer.hpp:208-215: they keep one stable scene_state shared_ptr and
// mutate it in place, bumping scene_state::version -- which is exactly why
// the memo key is the {pointer, version} PAIR and not the pointer alone.
//
// NO GPU. Everything asserted is the CPU merge/memo path: no RenderList, no
// QRhi, no window. The renderer's GPU entry points are stubbed empty and
// never invoked, so the test runs on hosts with no graphics stack at all
// (same approach as tests/unit/IsfUniformInputUsageTest.cpp's
// UpstreamRenderer).
//
// REGISTRATION (tests/gfx/CMakeLists.txt):
//
//     score_add_gfx_test(scene_merge_memo SceneMergeMemo.cpp)
//
// Linkage analysis: score::gfx::NodeRenderer and score::gfx::Node are both
// class-level SCORE_PLUGIN_GFX_EXPORT (NodeRenderer.hpp:10, Node.hpp:73), and
// rebuildMergedScene is only ever reached through the exported public
// process() overloads, so linking the real plug-in through
// test_gfx_engine_glue suffices. No score_plugin_hidden_sources
// recompilation is needed -- and per the ODR warning at
// tests/gfx/CMakeLists.txt:12-15 it must NOT be used when the symbols are
// exported.
//
// NEGATIVE CONTROLS (product-side, for reviewers -- do not commit):
//   * Delete the memo-hit early-return at NodeRenderer.cpp:500-504
//     (`if(sig == m_mergeCacheInputs && ...) { ...; return; }`): the idle
//     frames in "an idle frame re-runs merge_scenes zero times" re-merge
//     every delivery, the merged state pointer changes each time, and that
//     test goes red.
//   * Drop the version half of the memo key at NodeRenderer.cpp:495 --
//     `sig.push_back({s.state.get(), s.state->version})` ->
//     `sig.push_back({s.state.get(), 0})`: an in-place producer bump no
//     longer invalidates the memo, the stale cached merge is returned, and
//     "bumping one producer re-merges exactly once" goes red (the spec
//     phrases this control as making the idle-frame assertion red; in the
//     current code the idle frames stay green under this mutation -- it is
//     the bump-detection test that catches it).
// =============================================================================

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <vector>

using namespace score::gfx;

namespace
{
// GPU entry points stubbed empty; only the CPU process()/merge path is used.
struct MergeSinkRenderer final : NodeRenderer
{
  using NodeRenderer::NodeRenderer;

  void init(RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override { }
  void release(RenderList&) override { }
  void removeOutputPass(RenderList&, Edge&) override { }
};

struct PlainNode final : Node
{
  NodeRenderer* createRenderer(RenderList&) const noexcept override
  {
    return nullptr;
  }
};

// One upstream producer: a stable scene_state shared_ptr mutated in place,
// the pattern the memo key documents at NodeRenderer.hpp:208-215. `tag`
// provides a distinct, stable source_key address per producer.
struct Producer
{
  std::shared_ptr<ossia::scene_state> state;
  std::shared_ptr<ossia::scene_node> root; // null for env-only producers
  char tag{};

  const void* key() const noexcept { return &tag; }

  ossia::scene_spec spec() const
  {
    ossia::scene_spec s;
    s.state = state;
    return s;
  }

  void deliver(NodeRenderer& r) const { r.process(0, spec(), key()); }
};

Producer make_root_producer(const char* name, int64_t version)
{
  Producer p;
  p.root = std::make_shared<ossia::scene_node>();
  p.root->name = name;
  static uint64_t next_id = 1;
  p.root->id.value = next_id++;

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(p.root);

  p.state = std::make_shared<ossia::scene_state>();
  p.state->roots = std::move(roots);
  p.state->version = version;
  return p;
}

// EnvironmentLoader-style producer: no roots at all (scene_state::empty()
// is true, geometry_port.hpp:1297) but environment fields are set. The
// rebuild loop must keep it (NodeRenderer.cpp:488-498), and merge_scenes
// overlays its params_ambient group (geometry_port.cpp:412-419).
Producer make_env_only_producer(int64_t version)
{
  Producer p;
  p.state = std::make_shared<ossia::scene_state>();
  p.state->version = version;

  auto& env = p.state->environment;
  env.ambient_color[0] = 0.25f;
  env.ambient_color[1] = 0.50f;
  env.ambient_color[2] = 0.75f;
  env.ambient_intensity = 2.0f;
  env.params_set |= ossia::scene_environment::params_ambient;
  return p;
}

bool contains_root(const ossia::scene_spec& scene, const ossia::scene_node* n)
{
  if(!scene.state || !scene.state->roots)
    return false;
  for(const auto& r : *scene.state->roots)
    if(r.get() == n)
      return true;
  return false;
}

std::size_t root_count(const ossia::scene_spec& scene)
{
  return scene.state && scene.state->roots ? scene.state->roots->size() : 0;
}
}

TEST_CASE(
    "three producers into one Scene In port merge additively", "[gfx][scene]")
{
  PlainNode node;
  MergeSinkRenderer r{node};

  const auto p1 = make_root_producer("gltf", 1);
  const auto p2 = make_root_producer("camera", 2);
  const auto p3 = make_root_producer("light", 3);

  // (a) every contribution appears; the merged scene grows as producers
  // deliver on the SAME port with distinct source_keys
  // (NodeRenderer.cpp:590-596: one m_portScenes slot per (port, source)).
  p1.deliver(r);
  REQUIRE(r.scene.state);
  CHECK(contains_root(r.scene, p1.root.get()));

  p2.deliver(r);
  REQUIRE(r.scene.state);
  CHECK(root_count(r.scene) == 2);
  CHECK(contains_root(r.scene, p1.root.get()));
  CHECK(contains_root(r.scene, p2.root.get()));

  p3.deliver(r);
  REQUIRE(r.scene.state);
  CHECK(root_count(r.scene) == 3);
  CHECK(contains_root(r.scene, p1.root.get()));
  CHECK(contains_root(r.scene, p2.root.get()));
  CHECK(contains_root(r.scene, p3.root.get()));

  // A real merge happened: the merged state is a fresh allocation
  // (geometry_port.cpp:474), not any producer's own state.
  CHECK(r.scene.state != p1.state);
  CHECK(r.scene.state != p2.state);
  CHECK(r.scene.state != p3.state);

  // merge_scenes stamps version = max(inputs) + 1 (geometry_port.cpp:558).
  CHECK(r.scene.state->version == 4);

  // The memo signature tracked all three contributors (NodeRenderer.hpp:218).
  CHECK(r.m_mergeCacheInputs.size() == 3);

  // forEachSceneOnPort enumerates all three slots (NodeRenderer.hpp:161-166).
  int seen = 0;
  r.forEachSceneOnPort(0, [&](const ossia::scene_spec&) { ++seen; });
  CHECK(seen == 3);
}

TEST_CASE("an idle frame re-runs merge_scenes zero times", "[gfx][scene]")
{
  PlainNode node;
  MergeSinkRenderer r{node};

  const auto p1 = make_root_producer("gltf", 1);
  const auto p2 = make_root_producer("camera", 2);
  p1.deliver(r);
  p2.deliver(r);
  REQUIRE(r.scene.state);

  // Keep the merged state alive so a hypothetical re-merge can never
  // allocate at the same address (see the counting rationale up top).
  const auto merged_before = r.scene.state;

  // (b) idle frames: producers re-publish the identical scene_spec every
  // frame (the common case per the wrap-cache comment at
  // NodeRenderer.cpp:568-573). Same {pointer, version} signature => the
  // memo-hit branch at NodeRenderer.cpp:500-503 must re-publish the SAME
  // scene_state shared_ptr; a re-merge would produce a new allocation
  // (geometry_port.cpp:474) and this pointer would change.
  for(int frame = 0; frame < 5; ++frame)
  {
    p1.deliver(r);
    CHECK(r.scene.state.get() == merged_before.get());
    p2.deliver(r);
    CHECK(r.scene.state.get() == merged_before.get());
  }

  // The cached output is that same state (NodeRenderer.cpp:527).
  CHECK(r.m_mergeCacheOutput.state.get() == merged_before.get());
}

TEST_CASE("bumping one producer re-merges exactly once", "[gfx][scene]")
{
  PlainNode node;
  MergeSinkRenderer r{node};

  auto p1 = make_root_producer("gltf", 1);
  auto p2 = make_root_producer("camera", 2);
  const auto p3 = make_root_producer("light", 3);
  p1.deliver(r);
  p2.deliver(r);
  p3.deliver(r);
  REQUIRE(r.scene.state);
  const auto merged_v1 = r.scene.state; // held alive: no address reuse

  // (c) one producer mutates its stable scene_state in place and bumps
  // scene_state::version (geometry_port.hpp:1286) -- the exact producer
  // behavior the {pointer, version} memo key exists for
  // (NodeRenderer.hpp:208-216). The signature entry at NodeRenderer.cpp:495
  // now differs, so this single delivery must re-merge...
  p2.state->version = 7;
  p2.deliver(r);
  REQUIRE(r.scene.state);
  CHECK(r.scene.state.get() != merged_v1.get());
  const auto merged_v2 = r.scene.state;

  // ...into a scene that still carries all three contributions,
  CHECK(root_count(r.scene) == 3);
  CHECK(contains_root(r.scene, p1.root.get()));
  CHECK(contains_root(r.scene, p2.root.get()));
  CHECK(contains_root(r.scene, p3.root.get()));
  // with the recomputed version max(1, 7, 3) + 1 (geometry_port.cpp:558).
  CHECK(r.scene.state->version == 8);

  // ...and EXACTLY once: the very next idle deliveries (all producers,
  // including the one that bumped) are memo hits again -- the pointer
  // freezes on the post-bump merge result.
  for(int frame = 0; frame < 3; ++frame)
  {
    p1.deliver(r);
    CHECK(r.scene.state.get() == merged_v2.get());
    p2.deliver(r);
    CHECK(r.scene.state.get() == merged_v2.get());
    p3.deliver(r);
    CHECK(r.scene.state.get() == merged_v2.get());
  }
}

TEST_CASE(
    "a single contributor short-circuits to a pointer copy", "[gfx][scene]")
{
  PlainNode node;
  MergeSinkRenderer r{node};

  const auto p1 = make_root_producer("gltf", 5);
  p1.deliver(r);

  // (d) valid.size() == 1 => `this->scene = *valid[0]`
  // (NodeRenderer.cpp:513-518): the published scene_state is the producer's
  // own shared_ptr, byte-for-byte -- no merge_scenes call, no new
  // allocation, and in particular NO version re-stamp (a merge would have
  // produced version max+1 = 6, a fresh state, geometry_port.cpp:474,558).
  REQUIRE(r.scene.state);
  CHECK(r.scene.state.get() == p1.state.get());
  CHECK(r.scene.state->version == 5);
  CHECK(r.m_mergeCacheOutput.state.get() == p1.state.get());
  CHECK(r.m_mergeCacheInputs.size() == 1);

  // Idle re-delivery of the single contributor is a memo hit too.
  p1.deliver(r);
  CHECK(r.scene.state.get() == p1.state.get());
}

TEST_CASE(
    "an env-only producer with empty roots is not dropped", "[gfx][scene]")
{
  PlainNode node;
  MergeSinkRenderer r{node};

  const auto geo = make_root_producer("gltf", 1);
  auto env = make_env_only_producer(2);
  REQUIRE(env.state->empty()); // the property under test, geometry_port.hpp:1297

  geo.deliver(r);
  env.deliver(r);
  REQUIRE(r.scene.state);

  // (e) the rebuild filter keeps any non-null state, roots or not
  // (NodeRenderer.cpp:488-498). Had the env-only producer been dropped,
  // valid.size() would be 1 and the short-circuit at NodeRenderer.cpp:513-518
  // would publish geo's state pointer verbatim. Instead both contribute:
  CHECK(r.scene.state.get() != geo.state.get());
  CHECK(r.scene.state.get() != env.state.get());
  CHECK(r.m_mergeCacheInputs.size() == 2);

  // The geometry root survives, and only it (env contributes no roots).
  CHECK(root_count(r.scene) == 1);
  CHECK(contains_root(r.scene, geo.root.get()));

  // The environment overlay from the rootless producer is visible in the
  // merged scene (params_ambient group, geometry_port.cpp:412-419).
  const auto& menv = r.scene.state->environment;
  CHECK((menv.params_set & ossia::scene_environment::params_ambient) != 0);
  CHECK(menv.ambient_color[0] == 0.25f);
  CHECK(menv.ambient_color[1] == 0.50f);
  CHECK(menv.ambient_color[2] == 0.75f);
  CHECK(menv.ambient_intensity == 2.0f);

  // And an env-only bump (a fog slider, an exposure change...) invalidates
  // the memo like any other producer: in-place mutation + version bump =>
  // exactly one re-merge carrying the new value.
  const auto merged_v1 = r.scene.state;
  env.state->environment.ambient_intensity = 3.5f;
  env.state->version = 9;
  env.deliver(r);
  REQUIRE(r.scene.state);
  CHECK(r.scene.state.get() != merged_v1.get());
  CHECK(r.scene.state->environment.ambient_intensity == 3.5f);

  const auto merged_v2 = r.scene.state;
  geo.deliver(r); // idle frame afterwards: memo hit again
  CHECK(r.scene.state.get() == merged_v2.get());
}

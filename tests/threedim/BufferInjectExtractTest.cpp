// Threedim::InjectBuffer + Threedim::ExtractSceneBuffer — the aux-buffer
// injection / scene-slot extraction pair.
//
// InjectBuffer is pure scene_state -> scene_state algebra (rebuild() +
// operator()() reference no GPU symbol at all): clone the input state, drop
// same-name inject entries, append {name, native_handle, byte_size}. The
// contracts that matter downstream, same family as MaterialOverrideTest.cpp /
// Transform3DCompose.cpp:
//   - unwired (null handle / empty name / null scene) -> exact passthrough,
//     same shared_ptr, no clone;
//   - handle + byte_size land verbatim in the appended aux_inject_buffer;
//   - last-wins: a pre-existing same-name entry is removed, others survive;
//   - the input state is never mutated (copy-on-write clone), and the clone
//     shares the sub-vectors (roots etc.) by shared_ptr identity;
//   - an unchanged tick must NOT re-dirty and must republish the SAME
//     shared_ptr — the identity fast-path ScenePreprocessor keys on
//     (the plugin's established defect class, cf. TagAs, 8ad12fe91a);
//   - live handle / byte_size / in-place version changes are detected in
//     operator()() without a port event (the header documents why).
//
// ExtractSceneBuffer's slot resolution (pickSlotRef + isLive) runs on the
// render thread against a live GpuResourceRegistry, so the success path and
// the miss paths PAST the null-scene guard (bad camera/material index, stale
// ref) are out of scope here: they are only observable through
// renderer.registry(), which needs a fully-initialised RenderList — the
// ExtractComputeSrb.cpp GPU fixture shape, not a unit seam. What IS app-free:
//   - operator()() drains the execution-thread dirty flag;
//   - init()/release() clear the outlet and provably never touch their
//     RenderList& / QRhiResourceUpdateBatch& arguments;
//   - update() with a null scene clears the outlet BEFORE the first
//     renderer dereference.
// For those we hand the callees references into inert storage they are
// contractually forbidden to touch — the CameraRelease.cpp technique.
//
// Two defects were found while writing this, both fixed since (the cases
// below assert the correct behaviour and run green):
//   1. InjectBuffer's unwired first tick used `!m_cached_out` as a rebuild
//      trigger, so a null-scene node rebuilt and re-dirtied 0xFF on EVERY
//      tick (same subclass as MaterialOverrideTest.cpp's defect #1).
//   2. ExtractSceneBuffer's clear paths assigned a default gpu_buffer, so
//      a live handle vanishing (scene unplugged) published handle==nullptr
//      with changed==false — while the success path carefully computes
//      `changed` from prev-vs-new precisely so downstream rebinds key on it.

#include <Threedim/ExtractSceneBuffer.hpp>
#include <Threedim/InjectBuffer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace
{
ossia::scene_node_ptr make_node(const char* name, uint64_t id)
{
  auto n = std::make_shared<ossia::scene_node>();
  n->name = name;
  n->id.value = id;
  n->children = std::make_shared<std::vector<ossia::scene_payload>>();
  return n;
}

std::shared_ptr<ossia::scene_state>
make_state(std::vector<ossia::scene_node_ptr> roots, int64_t version = 1)
{
  auto s = std::make_shared<ossia::scene_state>();
  s->roots
      = std::make_shared<std::vector<ossia::scene_node_ptr>>(std::move(roots));
  s->version = version;
  return s;
}

const ossia::aux_inject_buffer*
find_aux(const ossia::scene_state& s, std::string_view name)
{
  for(auto& e : s.inject_buffers)
    if(e.name == name)
      return &e;
  return nullptr;
}

std::size_t count_aux(const ossia::scene_state& s, std::string_view name)
{
  std::size_t n = 0;
  for(auto& e : s.inject_buffers)
    if(e.name == name)
      ++n;
  return n;
}

// Distinct, never-dereferenced fake GPU handles. Everything downstream of
// the halp::gpu_buffer treats .handle as opaque, so plain locals suffice
// (the MaterialOverrideTest.cpp dummy-pointer pattern).
int dummy_a, dummy_b, dummy_c;
} // namespace

// ================================================================ InjectBuffer

TEST_CASE(
    "InjectBuffer passes the input scene through untouched while unwired",
    "[threedim][injectbuffer]")
{
  auto in_state = make_state({make_node("root", 1)}, /*version*/ 42);

  Threedim::InjectBuffer node;
  node.inputs.scene_in.scene.state = in_state;

  SECTION("no buffer handle")
  {
    node.inputs.aux_name.value = "scene_params";
    // buffer.handle stays null
  }
  SECTION("buffer wired but no aux name")
  {
    node.inputs.buffer.buffer.handle = &dummy_a;
    node.inputs.buffer.buffer.byte_size = 64;
    // aux_name stays ""
  }

  node();

  // Exact passthrough: SAME shared_ptr, no clone, nothing appended, input
  // version untouched.
  CHECK(node.outputs.scene_out.scene.state == in_state);
  CHECK(in_state->inject_buffers.empty());
  CHECK(in_state->version == 42);

  // First observation of the pass-through is announced ...
  CHECK(node.outputs.scene_out.dirty == 0xFF);
  // ... and an unchanged tick is silent, with stable identity.
  node();
  CHECK(node.outputs.scene_out.dirty == 0);
  CHECK(node.outputs.scene_out.scene.state == in_state);
}

TEST_CASE(
    "InjectBuffer appends the named injection with handle+size passthrough",
    "[threedim][injectbuffer]")
{
  auto in_state = make_state({make_node("root", 1)}, /*version*/ 42);

  Threedim::InjectBuffer node;
  node.inputs.scene_in.scene.state = in_state;
  node.inputs.buffer.buffer.handle = &dummy_a;
  node.inputs.buffer.buffer.byte_size = 128;
  node.inputs.aux_name.value = "scene_params";

  node();

  auto out = node.outputs.scene_out.scene.state;
  REQUIRE(out != nullptr);

  // Copy-on-write: a fresh state object, input untouched...
  CHECK(out != in_state);
  CHECK(in_state->inject_buffers.empty());
  CHECK(in_state->version == 42);
  // ...that still SHARES the sub-vectors by identity (shallow clone — the
  // preprocessor's per-subtree caches stay keyed on the same pointers).
  CHECK(out->roots == in_state->roots);

  // The injection: name + handle + byte_size land verbatim, nothing else.
  REQUIRE(out->inject_buffers.size() == 1);
  auto* aux = find_aux(*out, "scene_params");
  REQUIRE(aux);
  CHECK(aux->native_handle == &dummy_a);
  CHECK(aux->byte_size == 128);

  // Version discipline: node-local counter, dirty_index rides along.
  CHECK(out->version == 1);
  CHECK(out->dirty_index == out->version);
  CHECK(node.outputs.scene_out.dirty == 0xFF);

  // Unchanged tick: no re-dirty, and the SAME shared_ptr is republished
  // (the fingerprint fast-path contract from the header).
  node();
  CHECK(node.outputs.scene_out.dirty == 0);
  CHECK(node.outputs.scene_out.scene.state == out);
  node();
  CHECK(node.outputs.scene_out.dirty == 0);
  CHECK(node.outputs.scene_out.scene.state == out);
}

TEST_CASE(
    "InjectBuffer last-wins replaces same-name entries and preserves others",
    "[threedim][injectbuffer]")
{
  auto in_state = make_state({make_node("root", 1)}, /*version*/ 5);
  // The scene already publishes two auxes — e.g. ScenePreprocessor's own
  // scene_lights plus an earlier InjectBuffer's scene_params.
  in_state->inject_buffers.push_back(
      {.name = "scene_lights", .native_handle = &dummy_b, .byte_size = 512});
  in_state->inject_buffers.push_back(
      {.name = "scene_params", .native_handle = &dummy_c, .byte_size = 32});

  Threedim::InjectBuffer node;
  node.inputs.scene_in.scene.state = in_state;
  node.inputs.buffer.buffer.handle = &dummy_a;
  node.inputs.buffer.buffer.byte_size = 256;
  node.inputs.aux_name.value = "scene_params";

  node();

  auto out = node.outputs.scene_out.scene.state;
  REQUIRE(out != nullptr);
  REQUIRE(out->inject_buffers.size() == 2);

  // Foreign-name entry survives verbatim.
  auto* lights = find_aux(*out, "scene_lights");
  REQUIRE(lights);
  CHECK(lights->native_handle == &dummy_b);
  CHECK(lights->byte_size == 512);

  // Same-name entry: exactly one, ours, and appended LAST so flatten-time
  // last-wins resolution picks it.
  CHECK(count_aux(*out, "scene_params") == 1);
  auto* params = find_aux(*out, "scene_params");
  REQUIRE(params);
  CHECK(params->native_handle == &dummy_a);
  CHECK(params->byte_size == 256);
  CHECK(out->inject_buffers.back().name == "scene_params");

  // Input immutability: the old entry is still there, untouched.
  REQUIRE(in_state->inject_buffers.size() == 2);
  auto* old_params = find_aux(*in_state, "scene_params");
  REQUIRE(old_params);
  CHECK(old_params->native_handle == &dummy_c);
  CHECK(old_params->byte_size == 32);
}

TEST_CASE(
    "InjectBuffer detects live buffer and scene changes without a port event",
    "[threedim][injectbuffer]")
{
  auto in_state = make_state({make_node("root", 1)}, /*version*/ 1);

  Threedim::InjectBuffer node;
  node.inputs.scene_in.scene.state = in_state;
  node.inputs.buffer.buffer.handle = &dummy_a;
  node.inputs.buffer.buffer.byte_size = 128;
  node.inputs.aux_name.value = "aux";

  node();
  auto first = node.outputs.scene_out.scene.state;
  REQUIRE(first != nullptr);
  const int64_t v1 = first->version;

  // Handle swap (upstream producer reallocated): fresh state, new handle,
  // re-dirty, version bumped.
  node.inputs.buffer.buffer.handle = &dummy_b;
  node();
  auto second = node.outputs.scene_out.scene.state;
  REQUIRE(second != nullptr);
  CHECK(second != first);
  CHECK(node.outputs.scene_out.dirty == 0xFF);
  CHECK(second->version > v1);
  REQUIRE(find_aux(*second, "aux"));
  CHECK(find_aux(*second, "aux")->native_handle == &dummy_b);

  // Size-only change (handle stable) is downstream-observable too.
  node.inputs.buffer.buffer.byte_size = 256;
  node();
  auto third = node.outputs.scene_out.scene.state;
  REQUIRE(third != nullptr);
  CHECK(third != second);
  CHECK(node.outputs.scene_out.dirty == 0xFF);
  REQUIRE(find_aux(*third, "aux"));
  CHECK(find_aux(*third, "aux")->byte_size == 256);

  // In-place version bump on the SAME state pointer (producer mutates and
  // bumps): must be caught by the cached-version compare.
  in_state->version = 99;
  node();
  auto fourth = node.outputs.scene_out.scene.state;
  REQUIRE(fourth != nullptr);
  CHECK(fourth != third);
  CHECK(node.outputs.scene_out.dirty == 0xFF);

  // And after all that churn, an unchanged tick is silent again.
  node();
  CHECK(node.outputs.scene_out.dirty == 0);
  CHECK(node.outputs.scene_out.scene.state == fourth);
}

TEST_CASE(
    "InjectBuffer aux-name change via the control callback drops the old name",
    "[threedim][injectbuffer]")
{
  auto in_state = make_state({make_node("root", 1)}, /*version*/ 1);

  Threedim::InjectBuffer node;
  node.inputs.scene_in.scene.state = in_state;
  node.inputs.buffer.buffer.handle = &dummy_a;
  node.inputs.buffer.buffer.byte_size = 64;
  node.inputs.aux_name.value = "old_name";

  node();
  REQUIRE(node.outputs.scene_out.scene.state);
  CHECK(find_aux(*node.outputs.scene_out.scene.state, "old_name"));

  // The binding layer delivers a name edit as update() -> rebuild(); the
  // next tick publishes it. The rebuild starts from the INPUT state, so no
  // stale "old_name" entry can leak into the new output.
  node.inputs.aux_name.value = "new_name";
  node.rebuild();
  node();

  auto out = node.outputs.scene_out.scene.state;
  REQUIRE(out != nullptr);
  CHECK(node.outputs.scene_out.dirty == 0xFF);
  REQUIRE(out->inject_buffers.size() == 1);
  CHECK(find_aux(*out, "new_name"));
  CHECK(find_aux(*out, "old_name") == nullptr);
}

// DEFECT: with a null scene input, rebuild()'s passthrough leaves
// m_cached_out null, and operator()()'s `!m_cached_out` re-arm cannot tell
// "computed null" from "never built" — so an unwired InjectBuffer runs
// rebuild() and republishes dirty == 0xFF on EVERY tick. Correct behaviour
// (asserted here): after the first announcement, an unchanged null-scene
// tick is silent, exactly like the unwired-but-scene-present case above.
// Same defect subclass as MaterialOverrideTest.cpp's #1 (cf. TagAs,
// 8ad12fe91a). Fix shape: a separate m_built flag (or caching the null in a
// sentinel) so the identity compare, not nullness, drives the re-arm.
TEST_CASE(
    "InjectBuffer does not re-dirty on unchanged ticks with a null scene",
    "[threedim][injectbuffer]")
{
  Threedim::InjectBuffer node; // nothing wired at all

  node();
  CHECK(node.outputs.scene_out.scene.state == nullptr);
  CHECK(node.outputs.scene_out.dirty == 0xFF); // first tick may announce

  node();
  CHECK(node.outputs.scene_out.scene.state == nullptr);
  CHECK(node.outputs.scene_out.dirty == 0);
}

// ========================================================= ExtractSceneBuffer

namespace
{
// init()/update(null scene)/release() provably never touch their RenderList&
// / QRhiResourceUpdateBatch& arguments (init and release ignore them
// entirely; update's null-scene guard returns before the first
// renderer.registry() call). Hand them references into inert, correctly
// aligned storage they are contractually forbidden to dereference — the
// CameraRelease.cpp technique.
alignas(std::max_align_t) unsigned char inert_storage[64]{};

score::gfx::RenderList& inert_renderlist()
{
  return *reinterpret_cast<score::gfx::RenderList*>(&inert_storage[0]);
}
QRhiResourceUpdateBatch& inert_batch()
{
  return *reinterpret_cast<QRhiResourceUpdateBatch*>(&inert_storage[0]);
}

void scribble_outlet(Threedim::ExtractSceneBuffer& node)
{
  node.outputs.buffer.buffer.handle = &dummy_a;
  node.outputs.buffer.buffer.byte_offset = 16;
  node.outputs.buffer.buffer.byte_size = 4096;
  node.outputs.buffer.buffer.changed = true;
}

bool outlet_cleared(const Threedim::ExtractSceneBuffer& node)
{
  auto& b = node.outputs.buffer.buffer;
  return b.handle == nullptr && b.byte_offset == 0 && b.byte_size == 0;
}
} // namespace

TEST_CASE(
    "ExtractSceneBuffer execution tick drains the input dirty flag",
    "[threedim][extractscenebuffer]")
{
  Threedim::ExtractSceneBuffer node;
  node.inputs.scene_in.dirty = 0xFF;
  node();
  CHECK(node.inputs.scene_in.dirty == 0);

  // Idempotent: a second tick with nothing new keeps it drained.
  node();
  CHECK(node.inputs.scene_in.dirty == 0);
}

TEST_CASE(
    "ExtractSceneBuffer init and release clear the outlet",
    "[threedim][extractscenebuffer]")
{
  Threedim::ExtractSceneBuffer node;

  scribble_outlet(node);
  node.init(inert_renderlist(), inert_batch());
  CHECK(outlet_cleared(node));

  scribble_outlet(node);
  node.release(inert_renderlist());
  CHECK(outlet_cleared(node));
}

TEST_CASE(
    "ExtractSceneBuffer update with no scene clears the outlet before any "
    "renderer access",
    "[threedim][extractscenebuffer]")
{
  Threedim::ExtractSceneBuffer node;
  REQUIRE(node.inputs.scene_in.scene.state == nullptr);

  // Simulate a previously-published live frame, then unplug the scene.
  scribble_outlet(node);
  node.update(inert_renderlist(), inert_batch(), nullptr);

  // Downstream must see the null handle (its documented fallback trigger),
  // regardless of kind/index selectors — none of them are consulted before
  // the null-scene guard.
  CHECK(outlet_cleared(node));
}

// DEFECT: the clear paths assign a default halp::gpu_buffer, whose
// `changed` is false — so when a LIVE handle vanishes (scene unplugged
// between frames), the outlet flips non-null -> null with changed == false.
// The success path computes `changed` from prev-vs-new precisely so
// downstream rebinds can short-circuit on it ("Flip `changed` only when
// something downstream-observable actually moved") — and a handle
// disappearing IS downstream-observable movement: a consumer keying on
// `changed` keeps its stale binding. Correct behaviour (asserted here): the
// live -> cleared transition publishes changed == true.
TEST_CASE(
    "ExtractSceneBuffer flags `changed` when a live outlet is cleared",
    "[threedim][extractscenebuffer]")
{
  Threedim::ExtractSceneBuffer node;

  // Previous frame published a live buffer...
  scribble_outlet(node);
  REQUIRE(node.outputs.buffer.buffer.handle != nullptr);

  // ...this frame the scene is gone.
  node.update(inert_renderlist(), inert_batch(), nullptr);
  CHECK(node.outputs.buffer.buffer.handle == nullptr);
  CHECK(node.outputs.buffer.buffer.changed);
}

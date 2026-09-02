// Threedim::MaterialOverride — the material-table rewrite stage.
//
// Pure scene_state -> scene_state algebra: rebuild() clones exactly the
// targeted materials (All / ByIndex), applies only the toggled factor
// overrides and the wired texture handles, and republishes everything else
// by shared_ptr identity. No GPU: the texture "handles" here are plain
// dummy pointers — applyTextureOverride only copies the void* into
// texture_ref.texture.native_handle and resets `source` so the
// ScenePreprocessor will treat the ref as DYNAMIC; nothing dereferences it.
//
// The identity contracts that matter downstream, same family as
// Transform3DCompose.cpp / SceneGraphOps.cpp:
//   - non-targeted materials and untoggled fields pass through untouched;
//   - the input state is never mutated (clones, not in-place writes);
//   - clone shared_ptr addresses stay stable across rebuilds (the
//     preprocessor's material-arena slots are keyed on them);
//   - an unchanged tick must NOT re-dirty — the plugin's established
//     defect class (cf. TagAs, 8ad12fe91a).
//
// Historical note: three defects were found while writing this (the
// idle-tick 0xFF re-dirty of the unconfigured passthrough and of a null
// input with a toggle set, and the configured rebuild dropping
// collections / time_seconds / variant fields). All fixed; the formerly
// [!shouldfail]-pinned cases at the bottom now assert the correct
// behaviour and run green.

#include <Threedim/MaterialOverride.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
//! A material with a recognizable value in every field the node can touch,
//! derived from `seed` so two materials never collide.
std::shared_ptr<ossia::material_component> make_material(float seed, uint64_t id)
{
  auto m = std::make_shared<ossia::material_component>();
  m->base_color_factor[0] = seed;
  m->base_color_factor[1] = seed + 0.01f;
  m->base_color_factor[2] = seed + 0.02f;
  m->base_color_factor[3] = seed + 0.03f;
  m->metallic_factor = seed + 0.04f;
  m->roughness_factor = seed + 0.05f;
  m->emissive_factor[0] = seed + 0.06f;
  m->emissive_factor[1] = seed + 0.07f;
  m->emissive_factor[2] = seed + 0.08f;
  m->emissive_strength = seed + 0.09f;
  m->stable_id = id;
  m->tag = "m" + std::to_string(id);
  // Loader-style file texture on the base color slot, so the DYNAMIC
  // override's `source.reset()` is observable.
  auto src = std::make_shared<ossia::texture_source>();
  src->file_path = "albedo.png";
  m->base_color_texture.source = std::move(src);
  return m;
}

std::shared_ptr<ossia::scene_state>
make_state(std::vector<ossia::material_component_ptr> mats, int64_t version = 1)
{
  auto root = std::make_shared<ossia::scene_node>();
  root->name = "root";
  root->id.value = 1;
  root->children = std::make_shared<std::vector<ossia::scene_payload>>();

  auto s = std::make_shared<ossia::scene_state>();
  s->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{root});
  s->materials
      = std::make_shared<std::vector<ossia::material_component_ptr>>(std::move(mats));
  // Non-null shared components so pointer-passthrough checks are meaningful.
  s->animations
      = std::make_shared<std::vector<ossia::animation_component_ptr>>();
  s->cameras = std::make_shared<std::vector<ossia::camera_component_ptr>>();
  s->skeletons
      = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
  s->active_camera_id.value = 42;
  s->version = version;
  return s;
}

const ossia::material_component& mat_of(const ossia::scene_state& s, std::size_t i)
{
  REQUIRE(s.materials);
  REQUIRE(s.materials->size() > i);
  REQUIRE((*s.materials)[i]);
  return *(*s.materials)[i];
}
} // namespace

// ==================================================================== All mode

TEST_CASE("All mode overrides exactly the toggled factors on every material",
          "[threedim][material_override]")
{
  auto m0 = make_material(0.10f, 11);
  auto m1 = make_material(0.30f, 12);
  auto raw = make_state({m0, m1}, 5);

  Threedim::MaterialOverride n;
  n.inputs.scene_in.scene.state = raw;
  n.inputs.use_base_color.value = true;
  n.inputs.base_r.value = 0.2f;
  n.inputs.base_g.value = 0.4f;
  n.inputs.base_b.value = 0.6f;
  n.inputs.base_a.value = 0.8f;
  n.inputs.use_roughness.value = true;
  n.inputs.roughness.value = 0.25f;
  n.inputs.use_emissive.value = true;
  n.inputs.em_r.value = 1.f;
  n.inputs.em_g.value = 2.f;
  n.inputs.em_b.value = 3.f;
  n.inputs.em_strength.value = 0.5f;
  // use_metallic stays off: metallic must survive from the loader.

  n();
  const auto out = n.outputs.scene_out.scene.state;
  REQUIRE(out);
  CHECK(out != raw);
  CHECK(out->version == 1); // node-local counter, first rebuild
  CHECK(n.outputs.scene_out.dirty == 0xFF);
  REQUIRE(out->materials);
  REQUIRE(out->materials->size() == 2);

  const ossia::material_component* srcs[2]{m0.get(), m1.get()};
  for(std::size_t i = 0; i < 2; ++i)
  {
    const auto& c = mat_of(*out, i);
    CHECK(&c != srcs[i]); // clone, not in-place write
    // Overridden fields.
    CHECK(c.base_color_factor[0] == Approx(0.2f));
    CHECK(c.base_color_factor[1] == Approx(0.4f));
    CHECK(c.base_color_factor[2] == Approx(0.6f));
    CHECK(c.base_color_factor[3] == Approx(0.8f));
    CHECK(c.roughness_factor == Approx(0.25f));
    CHECK(c.emissive_factor[0] == Approx(1.f));
    CHECK(c.emissive_factor[1] == Approx(2.f));
    CHECK(c.emissive_factor[2] == Approx(3.f));
    CHECK(c.emissive_strength == Approx(0.5f));
    // Untoggled fields survive from the source.
    CHECK(c.metallic_factor == Approx(srcs[i]->metallic_factor));
    CHECK(c.base_color_texture.source.get()
          == srcs[i]->base_color_texture.source.get());
    // Identity metadata inherited so downstream fingerprints see the
    // same logical material.
    CHECK(c.stable_id == srcs[i]->stable_id);
    CHECK(c.tag == srcs[i]->tag);
  }

  // Shared components pass through by identity; only materials is swapped.
  CHECK(out->roots.get() == raw->roots.get());
  CHECK(out->animations.get() == raw->animations.get());
  CHECK(out->cameras.get() == raw->cameras.get());
  CHECK(out->skeletons.get() == raw->skeletons.get());
  CHECK(out->active_camera_id.value == 42);

  // The input tree is untouched: same materials vector, original fields.
  CHECK(raw->materials.get() != out->materials.get());
  CHECK(raw->version == 5);
  CHECK(m0->base_color_factor[0] == Approx(0.10f));
  CHECK(m0->roughness_factor == Approx(0.15f));
  CHECK(m1->emissive_strength == Approx(0.39f));

  SECTION("swapping the upstream material list GCs stale clone-cache entries")
  {
    CHECK(n.m_clone_cache.size() == 2);

    auto m2 = make_material(0.50f, 13);
    n.inputs.scene_in.scene.state = make_state({m2}, 6);
    n();
    const auto out2 = n.outputs.scene_out.scene.state;
    REQUIRE(out2);
    REQUIRE(out2->materials->size() == 1);
    CHECK(mat_of(*out2, 0).base_color_factor[0] == Approx(0.2f));
    CHECK(n.m_clone_cache.size() == 1);
    CHECK(n.m_clone_cache.find(m2.get()) != n.m_clone_cache.end());
  }
}

// ================================================================ ByIndex mode

TEST_CASE("ByIndex targets one material; the rest pass through by identity",
          "[threedim][material_override]")
{
  auto m0 = make_material(0.10f, 21);
  auto m1 = make_material(0.30f, 22);
  auto m2 = make_material(0.50f, 23);
  auto raw = make_state({m0, m1, m2});

  Threedim::MaterialOverride n;
  n.inputs.scene_in.scene.state = raw;
  n.inputs.mode.value = Threedim::MaterialOverride::ByIndex;
  n.inputs.index.value = 1;
  n.inputs.use_metallic.value = true;
  n.inputs.metallic.value = 0.9f;

  n();
  const auto out = n.outputs.scene_out.scene.state;
  REQUIRE(out);
  REQUIRE(out->materials);
  REQUIRE(out->materials->size() == 3);

  // Non-targeted: the very same shared_ptrs, not copies.
  CHECK((*out->materials)[0].get() == m0.get());
  CHECK((*out->materials)[2].get() == m2.get());
  CHECK(m0->metallic_factor == Approx(0.14f));

  // Targeted: cloned, only metallic changed.
  const auto& c = mat_of(*out, 1);
  CHECK(&c != m1.get());
  CHECK(c.metallic_factor == Approx(0.9f));
  CHECK(c.roughness_factor == Approx(m1->roughness_factor));
  CHECK(c.base_color_factor[0] == Approx(m1->base_color_factor[0]));
  CHECK(c.stable_id == 22);
  CHECK(m1->metallic_factor == Approx(0.34f)); // source untouched

  SECTION("an out-of-range index overrides nothing")
  {
    n.inputs.index.value = 7;
    n.inputs.index.update(n); // what the host does on a port change
    n();
    const auto out2 = n.outputs.scene_out.scene.state;
    REQUIRE(out2);
    REQUIRE(out2->materials->size() == 3);
    const ossia::material_component* want[3]{m0.get(), m1.get(), m2.get()};
    for(std::size_t i = 0; i < 3; ++i)
      CHECK((*out2->materials)[i].get() == want[i]);
  }
}

// ======================================================== tick/version algebra

TEST_CASE("a configured override neither rebuilds nor re-bumps on idle ticks",
          "[threedim][material_override]")
{
  auto m0 = make_material(0.10f, 31);
  auto raw = make_state({m0}, 1);

  Threedim::MaterialOverride n;
  n.inputs.scene_in.scene.state = raw;
  n.inputs.use_base_color.value = true;
  n.inputs.base_r.value = 0.5f;

  n();
  const auto first = n.outputs.scene_out.scene.state;
  REQUIRE(first);
  CHECK(n.outputs.scene_out.dirty == 0xFF);
  const auto* clone0 = (*first->materials)[0].get();

  SECTION("idle ticks republish the same object with dirty == 0")
  {
    n();
    CHECK(n.outputs.scene_out.scene.state == first);
    CHECK(n.outputs.scene_out.dirty == 0);
    CHECK(n.outputs.scene_out.scene.state->version == first->version);
    n();
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("an upstream version bump rebuilds, keeping clone identity stable")
  {
    raw->version = 2;
    n();
    const auto second = n.outputs.scene_out.scene.state;
    REQUIRE(second);
    CHECK(second != first);
    CHECK(second->version == first->version + 1);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    // Same clone shared_ptr, mutated in place: the preprocessor's
    // material-arena slot keyed on this address must survive the rebuild.
    CHECK((*second->materials)[0].get() == clone0);
  }

  SECTION("toggling a factor off reverts the clone to the upstream value")
  {
    n.inputs.use_metallic.value = true;
    n.inputs.metallic.value = 0.9f;
    n.inputs.use_metallic.update(n);
    n();
    CHECK(mat_of(*n.outputs.scene_out.scene.state, 0).metallic_factor
          == Approx(0.9f));

    n.inputs.use_metallic.value = false;
    n.inputs.use_metallic.update(n);
    n();
    const auto& c = mat_of(*n.outputs.scene_out.scene.state, 0);
    CHECK(&c == clone0); // still the cached clone...
    CHECK(c.metallic_factor == Approx(m0->metallic_factor)); // ...reverted
    CHECK(c.base_color_factor[0] == Approx(0.5f)); // other override kept
  }
}

// ============================================================ texture handles

TEST_CASE("a wired texture becomes a source-less DYNAMIC ref on the clone",
          "[threedim][material_override]")
{
  auto m0 = make_material(0.10f, 41);
  auto raw = make_state({m0});

  int t1{}, t2{};
  Threedim::MaterialOverride n;
  n.inputs.scene_in.scene.state = raw;
  n.inputs.base_color_tex.texture.handle = &t1;

  n();
  const auto out = n.outputs.scene_out.scene.state;
  REQUIRE(out);
  const auto& c = mat_of(*out, 0);
  CHECK(c.base_color_texture.texture.native_handle == &t1);
  CHECK(c.base_color_texture.texture.bindless_index == 0);
  // source reset -> ScenePreprocessor's channelDynamicHandle() sees DYNAMIC.
  CHECK(c.base_color_texture.source == nullptr);
  // The other three slots and all factors pass through.
  CHECK(c.metallic_roughness_texture.texture.native_handle == nullptr);
  CHECK(c.normal_texture.texture.native_handle == nullptr);
  CHECK(c.emissive_texture.texture.native_handle == nullptr);
  CHECK(c.base_color_factor[0] == Approx(0.10f));
  // Source material keeps its loader texture.
  CHECK(m0->base_color_texture.source != nullptr);

  SECTION("a mid-stream native-handle swap is detected without a port event")
  {
    n.inputs.base_color_tex.texture.handle = &t2;
    n(); // no update() call: operator()() itself must notice
    const auto out2 = n.outputs.scene_out.scene.state;
    REQUIRE(out2);
    CHECK(out2 != out);
    CHECK(out2->version == out->version + 1);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    const auto& c2 = mat_of(*out2, 0);
    CHECK(&c2 == &c); // stable clone identity
    CHECK(c2.base_color_texture.texture.native_handle == &t2);

    n();
    CHECK(n.outputs.scene_out.scene.state == out2);
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("unwiring the last override reverts to full passthrough")
  {
    n.inputs.base_color_tex.texture.handle = nullptr;
    n();
    CHECK(n.outputs.scene_out.scene.state == raw);
  }
}

// ================================================================ empty scenes

TEST_CASE("a scene without materials passes through untouched",
          "[threedim][material_override]")
{
  Threedim::MaterialOverride n;
  n.inputs.use_base_color.value = true;

  SECTION("null input scene")
  {
    n();
    CHECK(n.outputs.scene_out.scene.state == nullptr);
  }

  SECTION("materials list absent")
  {
    auto raw = make_state({});
    raw->materials = nullptr;
    n.inputs.scene_in.scene.state = raw;
    n();
    CHECK(n.outputs.scene_out.scene.state == raw);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    n();
    CHECK(n.outputs.scene_out.scene.state == raw);
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("materials list empty")
  {
    auto raw = make_state({});
    n.inputs.scene_in.scene.state = raw;
    n();
    CHECK(n.outputs.scene_out.scene.state == raw);
    n();
    CHECK(n.outputs.scene_out.dirty == 0);
  }
}

// ============================================================== known defects

// rebuild()'s unconfigured passthrough (`!any_tex && !any_factor`) must
// update the identity cache before returning; otherwise operator()() sees
// `m_cached_in_state != in_state` on every tick, rebuilds, and re-emits
// dirty = 0xFF in the node's DEFAULT state — invalidating every
// identity-keyed cache downstream each frame (the TagAs defect class,
// 8ad12fe91a). Transform3D's idle-tick contract ("neither rebuilds nor
// bumps") is the house rule.
TEST_CASE("an unconfigured passthrough must not re-dirty on idle ticks",
          "[threedim][material_override]")
{
  auto raw = make_state({make_material(0.10f, 51)});
  Threedim::MaterialOverride n;
  n.inputs.scene_in.scene.state = raw;

  n();
  REQUIRE(n.outputs.scene_out.scene.state == raw);
  CHECK(n.outputs.scene_out.dirty == 0xFF);

  n();
  CHECK(n.outputs.scene_out.scene.state == raw);
  CHECK(n.outputs.scene_out.dirty == 0);
}

// With a factor toggled but no input scene, m_cached_out is legitimately
// null; a null cached output must not by itself trigger a rebuild, or the
// node re-emits dirty = 0xFF on every tick — dirty flags with no scene
// attached, forever. Transform3D emits dirty == 0 for a null input.
TEST_CASE("a null input with overrides configured must not dirty every tick",
          "[threedim][material_override]")
{
  Threedim::MaterialOverride n;
  n.inputs.use_metallic.value = true;

  n();
  REQUIRE(n.outputs.scene_out.scene.state == nullptr);
  n();
  CHECK(n.outputs.scene_out.scene.state == nullptr);
  CHECK(n.outputs.scene_out.dirty == 0);
}

// The configured rebuild must forward every shared scene_state field —
// not only roots / animations / cameras / skeletons / environment /
// active_camera_id but also collections, time_seconds,
// active_variant_index and variant_names — the silent-data-loss set
// Transform3DCompose.cpp pins for wrapSceneWithTransform ("dropping any
// of these silently loses data on every pass").
TEST_CASE("a configured rebuild must forward every shared scene_state field",
          "[threedim][material_override]")
{
  auto raw = make_state({make_material(0.10f, 61)});
  raw->collections
      = std::make_shared<std::vector<ossia::scene_collection_ptr>>();
  raw->time_seconds = 1.5;
  raw->active_variant_index = 3;
  raw->variant_names.push_back("hero");

  Threedim::MaterialOverride n;
  n.inputs.scene_in.scene.state = raw;
  n.inputs.use_base_color.value = true;

  n();
  const auto out = n.outputs.scene_out.scene.state;
  REQUIRE(out);
  REQUIRE(out != raw);

  CHECK(out->collections.get() == raw->collections.get());
  CHECK(out->time_seconds == Approx(1.5));
  CHECK(out->active_variant_index == 3);
  CHECK(out->variant_names.size() == 1);
}

// Threedim::Transform3D + TransformHelper.
//
// Both are pure QMatrix4x4 / scene_state logic. The two properties that matter
// downstream are (a) the wrap composes translate * rotate * scale in that order
// and leaves the input tree's identity alone, and (b) an unchanged tick does
// NOT bump the version — a spurious bump invalidates every identity-keyed
// cache in the preprocessor every frame.
//
// No GPU: xform_slot stays invalid throughout, so the render-thread paths in
// init/update/release are never entered — the same property
// tests/threedim/CameraRelease.cpp documents and relies on.

#include <Threedim/Transform3D.hpp>
#include <Threedim/TransformHelper.hpp>

#include <ossia/detail/variant.hpp>

#include <QMatrix4x4>
#include <QQuaternion>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

using Catch::Approx;

namespace
{
//! Duck-typed stand-in for the TRS control block every caller of
//! computeTRSMatrix / wrapSceneWithTransform exposes.
struct Vec3
{
  float x{}, y{}, z{};
};
struct Control
{
  Vec3 value;
};
struct TRSInputs
{
  Control position{};
  Control rotation{};
  Control scale{{1.f, 1.f, 1.f}};
};

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

const ossia::scene_transform& xform_of(const ossia::scene_node& n)
{
  REQUIRE(n.children);
  REQUIRE(!n.children->empty());
  auto* t = ossia::get_if<ossia::scene_transform>(&(*n.children)[0]);
  REQUIRE(t);
  return *t;
}
} // namespace

// =========================================================== computeTRSMatrix

TEST_CASE("computeTRSMatrix builds column-major translate*rotate*scale",
          "[threedim][transform]")
{
  TRSInputs in;
  in.position.value = {1.f, 2.f, 3.f};
  in.rotation.value = {0.f, 90.f, 0.f};
  in.scale.value = {2.f, 3.f, 4.f};

  float out[16]{};
  Threedim::CachedTRS cache{};
  CHECK(Threedim::computeTRSMatrix(in, out, cache));

  QMatrix4x4 expected;
  expected.translate(1.f, 2.f, 3.f);
  expected.rotate(QQuaternion::fromEulerAngles(0.f, 90.f, 0.f));
  expected.scale(2.f, 3.f, 4.f);
  for(int i = 0; i < 16; ++i)
    CHECK(out[i] == Approx(expected.constData()[i]).margin(1e-5));

  // Ordering is observable: applying to a unit +X point must scale, then
  // rotate, then translate.
  const QVector3D p = expected.map(QVector3D(1.f, 0.f, 0.f));
  CHECK(p.x() == Approx(1.f).margin(1e-4));
  CHECK(p.y() == Approx(2.f).margin(1e-4));
  CHECK(p.z() == Approx(1.f).margin(1e-4)); // 3 + (-2) from the Y rotation
}

TEST_CASE("computeTRSMatrix reports change only when a control moved",
          "[threedim][transform]")
{
  TRSInputs in;
  float out[16]{};
  Threedim::CachedTRS cache{};

  CHECK(Threedim::computeTRSMatrix(in, out, cache)); // first call: cache invalid
  CHECK(cache.valid);
  CHECK_FALSE(Threedim::computeTRSMatrix(in, out, cache));
  CHECK_FALSE(Threedim::computeTRSMatrix(in, out, cache));

  in.scale.value.y = 1.5f;
  CHECK(Threedim::computeTRSMatrix(in, out, cache));
  CHECK_FALSE(Threedim::computeTRSMatrix(in, out, cache));
}

TEST_CASE("transformChanged mirrors computeTRSMatrix without applying",
          "[threedim][transform]")
{
  TRSInputs in;
  Threedim::CachedTRS cache{};
  CHECK(Threedim::transformChanged(in, cache));

  float out[16]{};
  Threedim::computeTRSMatrix(in, out, cache);
  CHECK_FALSE(Threedim::transformChanged(in, cache));

  in.rotation.value.z = 0.001f;
  CHECK(Threedim::transformChanged(in, cache));
  // Non-mutating: asking twice must give the same answer.
  CHECK(Threedim::transformChanged(in, cache));
}

// ====================================================== wrapSceneWithTransform

TEST_CASE("wrapSceneWithTransform re-roots the input under one TRS parent",
          "[threedim][transform]")
{
  auto a = make_node("a", 1);
  auto b = make_node("b", 2);
  auto raw = make_state({a, b}, 7);

  TRSInputs in;
  in.position.value = {1.f, 2.f, 3.f};
  in.rotation.value = {0.f, 0.f, 90.f};
  in.scale.value = {2.f, 2.f, 2.f};

  Threedim::CachedTRS cache{};
  int64_t counter = 0;
  auto out = Threedim::wrapSceneWithTransform(raw, in, cache, counter);
  REQUIRE(out);
  REQUIRE(out->roots);
  REQUIRE(out->roots->size() == 1);

  const auto& parent = *(*out->roots)[0];
  REQUIRE(parent.children);
  REQUIRE(parent.children->size() == 3);

  const auto& t = xform_of(parent);
  CHECK(t.translation[0] == Approx(1.f));
  CHECK(t.translation[1] == Approx(2.f));
  CHECK(t.translation[2] == Approx(3.f));
  CHECK(t.scale[0] == Approx(2.f));

  const auto q = QQuaternion::fromEulerAngles(0.f, 0.f, 90.f);
  CHECK(t.rotation[0] == Approx(q.x()).margin(1e-5));
  CHECK(t.rotation[1] == Approx(q.y()).margin(1e-5));
  CHECK(t.rotation[2] == Approx(q.z()).margin(1e-5));
  CHECK(t.rotation[3] == Approx(q.scalar()).margin(1e-5));

  // Input roots are re-parented by identity, never cloned.
  const ossia::scene_node* want[2]{a.get(), b.get()};
  for(int i = 0; i < 2; ++i)
  {
    auto* kid = ossia::get_if<ossia::scene_node_ptr>(&(*parent.children)[i + 1]);
    REQUIRE(kid);
    CHECK(kid->get() == want[i]);
  }
  // Original state untouched.
  CHECK(raw->roots->size() == 2);
  CHECK(counter == 1);
  CHECK(out->version == 1);
}

TEST_CASE("wrapSceneWithTransform forwards every shared scene_state field",
          "[threedim][transform]")
{
  auto raw = make_state({make_node("a", 1)});
  auto mat = std::make_shared<ossia::material_component>();
  auto skel = std::make_shared<ossia::skeleton_component>();
  raw->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{mat});
  raw->skeletons = std::make_shared<std::vector<ossia::skeleton_component_ptr>>(
      std::vector<ossia::skeleton_component_ptr>{skel});
  raw->collections
      = std::make_shared<std::vector<ossia::scene_collection_ptr>>();
  raw->active_camera_id.value = 42;
  raw->time_seconds = 1.5;
  raw->active_variant_index = 3;
  raw->variant_names.push_back("hero");

  TRSInputs in;
  Threedim::CachedTRS cache{};
  int64_t counter = 0;
  auto out = Threedim::wrapSceneWithTransform(raw, in, cache, counter);
  REQUIRE(out);

  // Identity-preserving passthrough: dropping any of these silently loses
  // data on every TRS pass.
  CHECK(out->materials.get() == raw->materials.get());
  CHECK(out->skeletons.get() == raw->skeletons.get());
  CHECK(out->collections.get() == raw->collections.get());
  CHECK(out->cameras.get() == raw->cameras.get());
  CHECK(out->animations.get() == raw->animations.get());
  CHECK(out->active_camera_id.value == 42);
  CHECK(out->time_seconds == Approx(1.5));
  CHECK(out->active_variant_index == 3);
  REQUIRE(out->variant_names.size() == 1);
  CHECK(out->variant_names[0] == "hero");
}

TEST_CASE("wrapSceneWithTransform on a null input returns null",
          "[threedim][transform]")
{
  TRSInputs in;
  Threedim::CachedTRS cache{};
  int64_t counter = 0;
  CHECK(Threedim::wrapSceneWithTransform(nullptr, in, cache, counter) == nullptr);
  CHECK(counter == 0);
}

// ================================================================ Transform3D

TEST_CASE("Transform3D wraps its input and republishes it unchanged",
          "[threedim][transform]")
{
  Threedim::Transform3D n;
  auto a = make_node("a", 1);
  auto raw = make_state({a}, 5);
  n.inputs.scene_in.scene.state = raw;
  n.inputs.position.value = {0.f, 10.f, 0.f};
  n.inputs.scale.value = {1.f, 1.f, 1.f};

  n();
  const auto first = n.outputs.scene_out.scene.state;
  REQUIRE(first);
  CHECK(n.outputs.scene_out.dirty == 0xFF);
  REQUIRE(first->roots->size() == 1);
  const auto& parent = *(*first->roots)[0];
  CHECK(xform_of(parent).translation[1] == Approx(10.f));
  auto* kid = ossia::get_if<ossia::scene_node_ptr>(&(*parent.children)[1]);
  REQUIRE(kid);
  CHECK(kid->get() == a.get());

  SECTION("an idle tick neither rebuilds nor bumps the version")
  {
    n();
    CHECK(n.outputs.scene_out.scene.state == first);
    CHECK(n.outputs.scene_out.dirty == 0);
    CHECK(n.outputs.scene_out.scene.state->version == first->version);
  }

  SECTION("moving a control rebuilds and bumps")
  {
    n.inputs.position.value = {0.f, 20.f, 0.f};
    n();
    CHECK(n.outputs.scene_out.scene.state != first);
    CHECK(n.outputs.scene_out.scene.state->version == first->version + 1);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    CHECK(xform_of(*(*n.outputs.scene_out.scene.state->roots)[0]).translation[1]
          == Approx(20.f));
  }

  SECTION("an upstream version bump rebuilds even with unchanged controls")
  {
    raw->version = 6;
    n();
    CHECK(n.outputs.scene_out.scene.state != first);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
  }
}

TEST_CASE("Transform3D emits nothing for an absent or empty input",
          "[threedim][transform]")
{
  Threedim::Transform3D n;
  n.inputs.scale.value = {1.f, 1.f, 1.f};

  SECTION("null input")
  {
    n();
    CHECK(n.outputs.scene_out.scene.state == nullptr);
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("a rootless input is empty, not wrapped")
  {
    n.inputs.scene_in.scene.state = make_state({});
    n();
    CHECK(n.outputs.scene_out.scene.state == nullptr);
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("an input that goes empty clears the cached wrap")
  {
    auto raw = make_state({make_node("a", 1)});
    n.inputs.scene_in.scene.state = raw;
    n();
    REQUIRE(n.outputs.scene_out.scene.state);

    n.inputs.scene_in.scene.state = make_state({});
    n();
    CHECK(n.outputs.scene_out.scene.state == nullptr);

    // ...and a later non-empty input rebuilds fresh rather than replaying
    // the stale wrap.
    n.inputs.scene_in.scene.state = raw;
    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
  }
}

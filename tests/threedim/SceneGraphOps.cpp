// Behavioural coverage for the pure scene_state -> scene_state family added by
// 52104d297f (+ TagAs from f0a202a782): SceneSwitch, CameraSwitch, SceneGroup,
// SceneSelector, SceneDuplicator, TagAs.
//
// All of these are GPU-free: their init/update/release take a RenderList but
// the algebra lives entirely in rebuild()/operator()(). Nothing here touches
// a QRhi, so the arena slot refs stay invalid throughout and the render-thread
// paths are never entered — the property test_threedim_camera_release also
// relies on.
//
// Modelled on tests/unit/PrimitiveCloudTest.cpp's FormatOverride section: hand
// -build a scene_state, run the node, assert the output tree, node identity,
// sibling preservation and the version/dirty latch.

#include <Threedim/CameraSwitch.hpp>
#include <Threedim/SceneDuplicator.hpp>
#include <Threedim/SceneGroup.hpp>
#include <Threedim/SceneSelector.hpp>
#include <Threedim/SceneSwitch.hpp>
#include <Threedim/TagAs.hpp>

#include <ossia/detail/variant.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
ossia::scene_node_ptr
make_node(const char* name, uint64_t id, std::vector<ossia::scene_payload> kids = {})
{
  auto n = std::make_shared<ossia::scene_node>();
  n->name = name;
  n->id.value = id;
  n->children = std::make_shared<std::vector<ossia::scene_payload>>(std::move(kids));
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

ossia::scene_transform trs(float tx, float ty, float tz)
{
  ossia::scene_transform t;
  t.translation[0] = tx;
  t.translation[1] = ty;
  t.translation[2] = tz;
  t.rotation[3] = 1.f;
  t.scale[0] = t.scale[1] = t.scale[2] = 1.f;
  return t;
}

//! A one-root scene shaped the way CameraSwitch::extractCameraPose expects:
//! root->children == { scene_transform, camera_component_ptr }.
std::shared_ptr<ossia::scene_state> make_camera_state(
    float tx, float yfov, float znear, float zfar, int64_t version = 1)
{
  auto cam = std::make_shared<ossia::camera_component>();
  cam->projection = ossia::camera_projection::perspective;
  cam->yfov = yfov;
  cam->aspect_ratio = 1.f;
  cam->znear = znear;
  cam->zfar = zfar;

  return make_state(
      {make_node(
          "cam", 100 + uint64_t(tx),
          {trs(tx, 0.f, 0.f), ossia::camera_component_ptr(std::move(cam))})},
      version);
}

const ossia::scene_transform* first_transform(const ossia::scene_node& n)
{
  if(!n.children)
    return nullptr;
  for(const auto& c : *n.children)
    if(auto* t = ossia::get_if<ossia::scene_transform>(&c))
      return t;
  return nullptr;
}

const ossia::camera_component* first_camera(const ossia::scene_node& n)
{
  if(!n.children)
    return nullptr;
  for(const auto& c : *n.children)
    if(auto* p = ossia::get_if<ossia::camera_component_ptr>(&c))
      if(*p)
        return p->get();
  return nullptr;
}
} // namespace

// ================================================================ SceneSwitch

TEST_CASE("SceneSwitch republishes the selected input verbatim", "[threedim][scene][switch]")
{
  Threedim::SceneSwitch n;
  auto a = make_state({make_node("a", 1)});
  auto b = make_state({make_node("b", 2)});
  auto c = make_state({make_node("c", 3)});
  auto d = make_state({make_node("d", 4)});
  n.inputs.scene0.scene.state = a;
  n.inputs.scene1.scene.state = b;
  n.inputs.scene2.scene.state = c;
  n.inputs.scene3.scene.state = d;

  const std::shared_ptr<const ossia::scene_state> expected[4]{a, b, c, d};
  for(int i = 0; i < 4; ++i)
  {
    n.inputs.index.value = i;
    n();
    CHECK(n.outputs.scene_out.scene.state == expected[i]);
  }
}

TEST_CASE("SceneSwitch out-of-range index falls back to slot 0, no crash",
          "[threedim][scene][switch]")
{
  Threedim::SceneSwitch n;
  auto a = make_state({make_node("a", 1)});
  n.inputs.scene0.scene.state = a;

  for(int idx : {-7, 4, 1000})
  {
    n.inputs.index.value = idx;
    n();
    CHECK(n.outputs.scene_out.scene.state == a);
  }
}

TEST_CASE("SceneSwitch raises dirty only on a real change", "[threedim][scene][switch]")
{
  Threedim::SceneSwitch n;
  auto a = make_state({make_node("a", 1)}, 5);
  auto b = make_state({make_node("b", 2)}, 5);
  n.inputs.scene0.scene.state = a;
  n.inputs.scene1.scene.state = b;

  n.inputs.index.value = 0;
  n();
  CHECK(n.outputs.scene_out.dirty == 0xFF); // first tick: index/state/version all new
  n();
  CHECK(n.outputs.scene_out.dirty == 0); // nothing moved

  SECTION("index switch")
  {
    n.inputs.index.value = 1;
    n();
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    n();
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("upstream version bump on the SAME state pointer")
  {
    a->version = 6;
    n();
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    n();
    CHECK(n.outputs.scene_out.dirty == 0);
  }
}

TEST_CASE("SceneSwitch selecting an unwired slot emits an empty, quiet scene",
          "[threedim][scene][switch]")
{
  Threedim::SceneSwitch n;
  n.inputs.scene0.scene.state = make_state({make_node("a", 1)});
  n.inputs.index.value = 2; // never wired

  n();
  CHECK(n.outputs.scene_out.scene.state == nullptr);
  CHECK(n.outputs.scene_out.dirty == 0);
}

// =============================================================== CameraSwitch

TEST_CASE("CameraSwitch Select forwards the picked camera scene",
          "[threedim][scene][cameraswitch]")
{
  Threedim::CameraSwitch n;
  n.inputs.mode.value = Threedim::CameraSwitch::ins::CameraMode::Select;

  auto c0 = make_camera_state(0.f, 1.0f, 0.1f, 100.f);
  auto c1 = make_camera_state(10.f, 0.5f, 0.2f, 200.f);
  n.inputs.cam0.scene.state = c0;
  n.inputs.cam1.scene.state = c1;

  n.inputs.index.value = 0;
  n();
  CHECK(n.outputs.scene_out.scene.state == c0);
  CHECK(n.outputs.scene_out.dirty == 0xFF);

  n.inputs.index.value = 1;
  n();
  CHECK(n.outputs.scene_out.scene.state == c1);

  SECTION("out-of-range clamps to slot 0")
  {
    n.inputs.index.value = 9;
    n();
    CHECK(n.outputs.scene_out.scene.state == c0);
  }
}

TEST_CASE("CameraSwitch Blend is a weighted mean of the poses",
          "[threedim][scene][cameraswitch]")
{
  Threedim::CameraSwitch n;
  n.inputs.mode.value = Threedim::CameraSwitch::ins::CameraMode::Blend;

  n.inputs.cam0.scene.state = make_camera_state(0.f, 1.0f, 0.1f, 100.f);
  n.inputs.cam1.scene.state = make_camera_state(4.f, 2.0f, 0.5f, 200.f);

  SECTION("full weight on one input reproduces that input's position")
  {
    n.inputs.weights.value = {1.f, 0.f, 0.f, 0.f};
    n.rebuild();
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    REQUIRE(st->roots);
    REQUIRE(st->roots->size() == 1);
    const auto* t = first_transform(*(*st->roots)[0]);
    REQUIRE(t);
    CHECK(t->translation[0] == Approx(0.f));
    CHECK(first_camera(*(*st->roots)[0]) != nullptr);
  }

  SECTION("equal weights land halfway; raw weights are normalised")
  {
    n.inputs.weights.value = {3.f, 3.f, 0.f, 0.f}; // not normalised on purpose
    n.rebuild();
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    const auto* t = first_transform(*(*st->roots)[0]);
    REQUIRE(t);
    CHECK(t->translation[0] == Approx(2.f));
  }

  SECTION("negative weights are clamped to zero, not folded in")
  {
    n.inputs.weights.value = {1.f, -5.f, 0.f, 0.f};
    n.rebuild();
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    const auto* t = first_transform(*(*st->roots)[0]);
    REQUIRE(t);
    CHECK(t->translation[0] == Approx(0.f));
  }

  SECTION("the blended camera keeps ONE stable node id across ticks")
  {
    n.inputs.weights.value = {1.f, 1.f, 0.f, 0.f};
    n.rebuild();
    n();
    const auto id0 = (*n.outputs.scene_out.scene.state->roots)[0]->id.value;
    CHECK(id0 != 0);
    CHECK(n.outputs.scene_out.scene.state->active_camera_id.value == id0);

    n.inputs.weights.value = {1.f, 3.f, 0.f, 0.f};
    n.rebuild();
    n();
    CHECK((*n.outputs.scene_out.scene.state->roots)[0]->id.value == id0);
  }
}

TEST_CASE("CameraSwitch Blend with no usable input empties the scene, loudly",
          "[threedim][scene][cameraswitch]")
{
  Threedim::CameraSwitch n;
  n.inputs.mode.value = Threedim::CameraSwitch::ins::CameraMode::Blend;
  n.inputs.cam0.scene.state = make_camera_state(1.f, 1.f, 0.1f, 10.f);

  n.inputs.weights.value = {1.f, 0.f, 0.f, 0.f};
  n.rebuild();
  n();
  REQUIRE(n.outputs.scene_out.scene.state);
  REQUIRE_FALSE(n.outputs.scene_out.scene.state->empty());
  const auto prev_version = n.outputs.scene_out.scene.state->version;

  // All weights off: the previous blend must not keep rendering.
  n.inputs.weights.value = {0.f, 0.f, 0.f, 0.f};
  n.rebuild();
  n();
  REQUIRE(n.outputs.scene_out.scene.state);
  CHECK(n.outputs.scene_out.scene.state->empty());
  CHECK(n.outputs.scene_out.scene.state->version > prev_version);
  CHECK(n.outputs.scene_out.dirty == 0xFF);
}

TEST_CASE("CameraSwitch Blend ignores an input carrying no camera",
          "[threedim][scene][cameraswitch]")
{
  Threedim::CameraSwitch n;
  n.inputs.mode.value = Threedim::CameraSwitch::ins::CameraMode::Blend;
  n.inputs.cam0.scene.state = make_camera_state(0.f, 1.f, 0.1f, 100.f);
  // A transform-only root: extractCameraPose must reject it.
  n.inputs.cam1.scene.state
      = make_state({make_node("no-cam", 7, {trs(50.f, 0.f, 0.f)})});

  n.inputs.weights.value = {1.f, 1.f, 0.f, 0.f};
  n.rebuild();
  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE_FALSE(st->empty());
  const auto* t = first_transform(*(*st->roots)[0]);
  REQUIRE(t);
  CHECK(t->translation[0] == Approx(0.f));
}

// ================================================================= SceneGroup

TEST_CASE("SceneGroup wraps every input root under one named parent",
          "[threedim][scene][group]")
{
  Threedim::SceneGroup n;
  auto r0 = make_node("mesh0", 1);
  auto r1 = make_node("mesh1", 2);
  auto r2 = make_node("mesh2", 3);
  n.inputs.scene0.scene.state = make_state({r0});
  n.inputs.scene1.scene.state = make_state({r1, r2});
  n.inputs.name.value = "ProsceniumSet";
  n.inputs.scale.value = {1.f, 1.f, 1.f};

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots);
  REQUIRE(st->roots->size() == 1);

  const auto& parent = *(*st->roots)[0];
  CHECK(parent.name == "ProsceniumSet");
  REQUIRE(parent.children);
  // [0] = the group TRS, then one payload per contributed root, in order.
  REQUIRE(parent.children->size() == 4);
  CHECK(ossia::get_if<ossia::scene_transform>(&(*parent.children)[0]));

  const ossia::scene_node* expect[3]{r0.get(), r1.get(), r2.get()};
  for(int i = 0; i < 3; ++i)
  {
    auto* kid = ossia::get_if<ossia::scene_node_ptr>(&(*parent.children)[i + 1]);
    REQUIRE(kid);
    CHECK(kid->get() == expect[i]); // identity preserved, not cloned
  }
}

TEST_CASE("SceneGroup dedups a root wired into more than one slot",
          "[threedim][scene][group]")
{
  Threedim::SceneGroup n;
  auto shared_root = make_node("asset", 1);
  auto s = make_state({shared_root});
  n.inputs.scene0.scene.state = s;
  n.inputs.scene1.scene.state = s;
  n.inputs.scene2.scene.state = make_state({shared_root}); // same node, new state
  n.inputs.scale.value = {1.f, 1.f, 1.f};

  n();
  const auto& parent = *(*n.outputs.scene_out.scene.state->roots)[0];
  REQUIRE(parent.children);
  CHECK(parent.children->size() == 2); // TRS + the single deduped root
}

TEST_CASE("SceneGroup applies its own TRS and defaults its name",
          "[threedim][scene][group]")
{
  Threedim::SceneGroup n;
  n.inputs.scene0.scene.state = make_state({make_node("a", 1)});
  n.inputs.position.value = {1.f, 2.f, 3.f};
  n.inputs.scale.value = {2.f, 4.f, 8.f};

  n();
  const auto& parent = *(*n.outputs.scene_out.scene.state->roots)[0];
  CHECK(parent.name == "Group");
  const auto* t = first_transform(parent);
  REQUIRE(t);
  CHECK(t->translation[0] == Approx(1.f));
  CHECK(t->translation[1] == Approx(2.f));
  CHECK(t->translation[2] == Approx(3.f));
  CHECK(t->scale[0] == Approx(2.f));
  CHECK(t->scale[1] == Approx(4.f));
  CHECK(t->scale[2] == Approx(8.f));
}

TEST_CASE("SceneGroup takes the first non-zero active camera and merges cameras",
          "[threedim][scene][group]")
{
  Threedim::SceneGroup n;
  auto camA = std::make_shared<ossia::camera_component>();
  auto camB = std::make_shared<ossia::camera_component>();

  auto s0 = make_state({make_node("a", 1)});
  s0->cameras = std::make_shared<std::vector<ossia::camera_component_ptr>>(
      std::vector<ossia::camera_component_ptr>{camA});
  // No active camera on the first input: the second one's must win.
  auto s1 = make_state({make_node("b", 2)});
  s1->cameras = std::make_shared<std::vector<ossia::camera_component_ptr>>(
      std::vector<ossia::camera_component_ptr>{camB});
  s1->active_camera_id.value = 77;
  auto s2 = make_state({make_node("c", 3)});
  s2->active_camera_id.value = 99;

  n.inputs.scene0.scene.state = s0;
  n.inputs.scene1.scene.state = s1;
  n.inputs.scene2.scene.state = s2;
  n.inputs.scale.value = {1.f, 1.f, 1.f};

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  CHECK(st->active_camera_id.value == 77);
  REQUIRE(st->cameras);
  REQUIRE(st->cameras->size() == 2);
  CHECK((*st->cameras)[0].get() == camA.get());
  CHECK((*st->cameras)[1].get() == camB.get());
}

TEST_CASE("SceneGroup only rebuilds when an upstream input actually moved",
          "[threedim][scene][group]")
{
  Threedim::SceneGroup n;
  auto s = make_state({make_node("a", 1)}, 3);
  n.inputs.scene0.scene.state = s;
  n.inputs.scale.value = {1.f, 1.f, 1.f};

  n();
  const auto first = n.outputs.scene_out.scene.state;
  REQUIRE(first);
  CHECK(n.outputs.scene_out.dirty == 0xFF);

  n();
  CHECK(n.outputs.scene_out.scene.state == first); // same object, no churn
  CHECK(n.outputs.scene_out.dirty == 0);

  s->version = 4;
  n();
  CHECK(n.outputs.scene_out.scene.state != first);
  CHECK(n.outputs.scene_out.scene.state->version == first->version + 1);
  CHECK(n.outputs.scene_out.dirty == 0xFF);
}

// ============================================================== SceneSelector

TEST_CASE("SceneSelector ByIndex picks a root and drops the siblings",
          "[threedim][scene][selector]")
{
  Threedim::SceneSelector n;
  auto r0 = make_node("a", 1);
  auto r1 = make_node("b", 2);
  n.inputs.scene_in.scene.state = make_state({r0, r1});
  n.inputs.mode.value = Threedim::SceneSelector::ByIndex;

  n.inputs.index.value = 1;
  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots);
  REQUIRE(st->roots->size() == 1);
  CHECK((*st->roots)[0].get() == r1.get());
}

TEST_CASE("SceneSelector out-of-range index yields an empty state, not null",
          "[threedim][scene][selector]")
{
  Threedim::SceneSelector n;
  n.inputs.scene_in.scene.state = make_state({make_node("a", 1)});
  n.inputs.mode.value = Threedim::SceneSelector::ByIndex;
  n.inputs.index.value = 42;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st); // a state object, with no roots
  REQUIRE(st->roots);
  CHECK(st->roots->empty());
  CHECK(st->empty());
}

TEST_CASE("SceneSelector ByName finds a nested node and keeps its subtree",
          "[threedim][scene][selector]")
{
  Threedim::SceneSelector n;
  auto leaf = make_node("Head", 3);
  auto mid = make_node("Torso", 2, {ossia::scene_node_ptr{leaf}});
  auto root = make_node("Rig", 1, {trs(0, 0, 0), ossia::scene_node_ptr{mid}});
  n.inputs.scene_in.scene.state = make_state({root});
  n.inputs.mode.value = Threedim::SceneSelector::ByName;

  SECTION("a nested match is returned with its own children intact")
  {
    n.inputs.path.value = "Torso";
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st->roots->size() == 1);
    const auto& found = (*st->roots)[0];
    CHECK(found.get() == mid.get());
    REQUIRE(found->children);
    REQUIRE(found->children->size() == 1);
    auto* kid = ossia::get_if<ossia::scene_node_ptr>(&(*found->children)[0]);
    REQUIRE(kid);
    CHECK(kid->get() == leaf.get());
  }

  SECTION("no match yields a valid empty state")
  {
    n.inputs.path.value = "Nonexistent";
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    REQUIRE(st->roots);
    CHECK(st->roots->empty());
  }
}

TEST_CASE("SceneSelector ByPath matches globs against the /root/child path",
          "[threedim][scene][selector]")
{
  Threedim::SceneSelector n;
  auto leaf = make_node("Head", 3);
  auto mid = make_node("Torso", 2, {ossia::scene_node_ptr{leaf}});
  auto root = make_node("Rig", 1, {ossia::scene_node_ptr{mid}});
  n.inputs.scene_in.scene.state = make_state({root});
  n.inputs.mode.value = Threedim::SceneSelector::ByPath;

  SECTION("exact path")
  {
    n.inputs.path.value = "/Rig/Torso";
    n();
    REQUIRE(n.outputs.scene_out.scene.state->roots->size() == 1);
    CHECK((*n.outputs.scene_out.scene.state->roots)[0].get() == mid.get());
  }

  SECTION("** crosses separators, * does not")
  {
    n.inputs.path.value = "/Rig/**/Head";
    n();
    REQUIRE(n.outputs.scene_out.scene.state->roots->size() == 1);
    CHECK((*n.outputs.scene_out.scene.state->roots)[0].get() == leaf.get());

    n.inputs.path.value = "/Rig/*/Head";
    n.rebuild();
    n();
    CHECK(n.outputs.scene_out.scene.state->roots->size() == 1);

    n.inputs.path.value = "/*";
    n.rebuild();
    n();
    // A single * cannot span the '/' between Rig and Torso, so only the
    // root itself matches.
    REQUIRE(n.outputs.scene_out.scene.state->roots->size() == 1);
    CHECK((*n.outputs.scene_out.scene.state->roots)[0].get() == root.get());
  }
}

TEST_CASE("SceneSelector ZeroOut drops the subtree's leading transform",
          "[threedim][scene][selector]")
{
  Threedim::SceneSelector n;
  auto target = make_node("Prop", 2, {trs(5.f, 6.f, 7.f), ossia::scene_node_ptr{}});
  n.inputs.scene_in.scene.state = make_state({target});
  n.inputs.mode.value = Threedim::SceneSelector::ByIndex;
  n.inputs.index.value = 0;

  SECTION("Preserve keeps the node verbatim")
  {
    n.inputs.rebase.value = Threedim::SceneSelector::Preserve;
    n();
    const auto& found = (*n.outputs.scene_out.scene.state->roots)[0];
    CHECK(found.get() == target.get());
    CHECK(first_transform(*found) != nullptr);
  }

  SECTION("ZeroOut clones without the TRS and keeps the remaining siblings")
  {
    n.inputs.rebase.value = Threedim::SceneSelector::ZeroOut;
    n.rebuild();
    n();
    const auto& found = (*n.outputs.scene_out.scene.state->roots)[0];
    CHECK(found.get() != target.get()); // a clone
    CHECK(found->name == "Prop");
    CHECK(first_transform(*found) == nullptr);
    REQUIRE(found->children);
    CHECK(found->children->size() == 1);
    // Original untouched.
    CHECK(target->children->size() == 2);
    CHECK(first_transform(*target) != nullptr);
  }
}

TEST_CASE("SceneSelector carries the non-root scene fields through",
          "[threedim][scene][selector]")
{
  Threedim::SceneSelector n;
  auto mat = std::make_shared<ossia::material_component>();
  auto in = make_state({make_node("a", 1)}, 11);
  in->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{mat});
  in->dirty_index = 4;
  in->active_camera_id.value = 55;
  n.inputs.scene_in.scene.state = in;
  n.inputs.mode.value = Threedim::SceneSelector::ByIndex;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  CHECK(st->materials.get() == in->materials.get());
  CHECK(st->active_camera_id.value == 55);
  CHECK(st->dirty_index == 5);
  CHECK(st->version == 1); // the node's own monotone counter, not the input's
}

// ============================================================ SceneDuplicator

TEST_CASE("SceneDuplicator emits N clone roots sharing the prototype",
          "[threedim][scene][duplicator]")
{
  Threedim::SceneDuplicator n;
  auto proto = make_node("Chair", 1);
  n.inputs.scene_in.scene.state = make_state({proto});
  n.inputs.count.value = 3;
  n.inputs.pattern.value = Threedim::SceneDuplicator::Line;
  n.inputs.spacing.value = 2.f;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots);
  REQUIRE(st->roots->size() == 3);

  for(int i = 0; i < 3; ++i)
  {
    const auto& clone = (*st->roots)[i];
    CHECK(clone->name == "Chair_" + std::to_string(i));
    REQUIRE(clone->children);
    REQUIRE(clone->children->size() == 2);
    CHECK(ossia::get_if<ossia::scene_transform>(&(*clone->children)[0]));
    auto* kid = ossia::get_if<ossia::scene_node_ptr>(&(*clone->children)[1]);
    REQUIRE(kid);
    CHECK(kid->get() == proto.get()); // the prototype is shared, not copied
  }

  // Line pattern: centred on the origin, spacing apart.
  const float xs[3]{-2.f, 0.f, 2.f};
  for(int i = 0; i < 3; ++i)
  {
    const auto* t = first_transform(*(*st->roots)[i]);
    REQUIRE(t);
    CHECK(t->translation[0] == Approx(xs[i]));
    CHECK(t->translation[1] == Approx(0.f));
    CHECK(t->translation[2] == Approx(0.f));
  }

  // Original untouched.
  CHECK(n.inputs.scene_in.scene.state->roots->size() == 1);
  CHECK((*n.inputs.scene_in.scene.state->roots)[0].get() == proto.get());
}

TEST_CASE("SceneDuplicator Ring places clones on a circle of `radius`",
          "[threedim][scene][duplicator]")
{
  Threedim::SceneDuplicator n;
  n.inputs.scene_in.scene.state = make_state({make_node("P", 1)});
  n.inputs.count.value = 4;
  n.inputs.pattern.value = Threedim::SceneDuplicator::Ring;
  n.inputs.radius.value = 5.f;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st->roots->size() == 4);
  for(int i = 0; i < 4; ++i)
  {
    const auto* t = first_transform(*(*st->roots)[i]);
    REQUIRE(t);
    const float r = std::hypot(t->translation[0], t->translation[2]);
    CHECK(r == Approx(5.f).margin(1e-4));
    CHECK(t->translation[1] == Approx(0.f));
  }
}

TEST_CASE("SceneDuplicator count is clamped to at least one",
          "[threedim][scene][duplicator]")
{
  Threedim::SceneDuplicator n;
  n.inputs.scene_in.scene.state = make_state({make_node("P", 1)});
  n.inputs.pattern.value = Threedim::SceneDuplicator::Line;

  for(int c : {0, -3})
  {
    n.inputs.count.value = c;
    n.rebuild();
    n();
    REQUIRE(n.outputs.scene_out.scene.state->roots->size() == 1);
  }
}

TEST_CASE("SceneDuplicator shares the prototype's non-root resources",
          "[threedim][scene][duplicator]")
{
  Threedim::SceneDuplicator n;
  auto in = make_state({make_node("P", 1)});
  auto mat = std::make_shared<ossia::material_component>();
  in->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{mat});
  in->active_camera_id.value = 12;
  n.inputs.scene_in.scene.state = in;
  n.inputs.count.value = 2;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  CHECK(st->materials.get() == in->materials.get());
  CHECK(st->active_camera_id.value == 12);
}

TEST_CASE("SceneDuplicator passes an empty input straight through",
          "[threedim][scene][duplicator]")
{
  Threedim::SceneDuplicator n;
  n.inputs.count.value = 4;

  SECTION("null input")
  {
    n();
    CHECK(n.outputs.scene_out.scene.state == nullptr);
  }

  SECTION("rootless input")
  {
    auto in = make_state({});
    n.inputs.scene_in.scene.state = in;
    n();
    CHECK(n.outputs.scene_out.scene.state == in);
  }
}

// ===================================================================== TagAs

TEST_CASE("TagAs stamps format_id onto every reachable cloud",
          "[threedim][scene][tagas]")
{
  auto cloud = std::make_shared<ossia::primitive_cloud_component>();
  cloud->primitive_count = 4;
  cloud->row_stride = 32;
  auto storage = std::shared_ptr<uint8_t[]>(new uint8_t[128]());
  auto br = std::make_shared<ossia::buffer_resource>();
  br->resource = ossia::buffer_data{
      .data = std::shared_ptr<const void>(storage, storage.get()),
      .byte_size = 128,
      .usage_hint = ossia::buffer_data::usage::storage_buffer};
  cloud->raw_data = br;

  auto in = make_state(
      {make_node(
          "cloud", 1,
          {trs(0, 0, 0), ossia::primitive_cloud_component_ptr{cloud}})},
      3);

  Threedim::TagAs n;
  n.inputs.scene_in.scene.state = in;
  n.inputs.format_id.value = "my-custom-format";

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  CHECK(st.get() != in.get());
  CHECK(st->version == in->version + 1);
  CHECK(n.outputs.scene_out.dirty == 0xFF);

  const auto& root = (*st->roots)[0];
  REQUIRE(root->children);
  REQUIRE(root->children->size() == 2);
  // Sibling transform survives.
  CHECK(ossia::get_if<ossia::scene_transform>(&(*root->children)[0]));
  auto* pc
      = ossia::get_if<ossia::primitive_cloud_component_ptr>(&(*root->children)[1]);
  REQUIRE(pc);
  CHECK((*pc)->format_id == "my-custom-format");
  CHECK((*pc)->raw_data.get() == br.get()); // payload shared, not copied
  CHECK((*pc)->primitive_count == 4);

  // Upstream untouched.
  CHECK(cloud->format_id.empty());

  // Second tick: no upstream change, republish the same object, dirty clears.
  n();
  CHECK(n.outputs.scene_out.scene.state == st);
  CHECK(n.outputs.scene_out.dirty == 0);
}

TEST_CASE("TagAs with an empty format id is a passthrough",
          "[threedim][scene][tagas]")
{
  auto in = make_state({make_node("a", 1)});
  Threedim::TagAs n;
  n.inputs.scene_in.scene.state = in;
  n.inputs.format_id.value = "";

  n();
  CHECK(n.outputs.scene_out.scene.state.get() == in.get());
}

TEST_CASE("TagAs handles a null upstream", "[threedim][scene][tagas]")
{
  Threedim::TagAs n;
  n.inputs.format_id.value = "x";
  n();
  CHECK(n.outputs.scene_out.scene.state == nullptr);
  CHECK(n.outputs.scene_out.dirty == 0xFF);
}

TEST_CASE("TagAs re-runs when the upstream state pointer changes",
          "[threedim][scene][tagas]")
{
  Threedim::TagAs n;
  n.inputs.format_id.value = "fmt";
  auto a = make_state({make_node("a", 1)}, 1);
  n.inputs.scene_in.scene.state = a;
  n();
  const auto out_a = n.outputs.scene_out.scene.state;
  REQUIRE(out_a);

  auto b = make_state({make_node("b", 2)}, 1); // same version, different object
  n.inputs.scene_in.scene.state = b;
  n();
  CHECK(n.outputs.scene_out.scene.state != out_a);
  CHECK(n.outputs.scene_out.dirty == 0xFF);
}

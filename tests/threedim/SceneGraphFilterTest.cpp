// Threedim::SceneGraphFilter — the scene_state -> scene_state culling node.
//
// Pure CPU: the whole unit lives in rebuild()/operator()(). No QRhi is ever
// touched, so this runs headless like the rest of the scene-graph algebra
// suite (SceneGraphOps.cpp).
//
// The properties that matter downstream:
//  (a) per-mode predicate semantics: keep matching nodes, drop the rest;
//      Invert flips the list into an exclude filter; a non-matching ancestor
//      survives as a wrapper when a descendant matches;
//  (b) structural sharing: subtrees the walk leaves untouched come back by
//      shared_ptr identity so downstream identity-keyed caches stay warm;
//  (c) tick discipline: a real change bumps the version and raises dirty,
//      an unchanged tick must NOT re-bump — the same spurious-re-dirty
//      defect class the TagAs / Transform3D pins in SceneGraphOps.cpp and
//      Transform3DCompose.cpp guard against.
//
// Three [!shouldfail] cases document real defects (marked DEFECT below);
// they assert the CORRECT behaviour and flip to green the day each is fixed.

#include <Threedim/SceneGraphFilter.hpp>

#include <ossia/detail/variant.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

namespace
{
using Filter = Threedim::SceneGraphFilter;

std::shared_ptr<ossia::scene_node>
make_node(const char* name, uint64_t id, std::vector<ossia::scene_payload> kids = {})
{
  auto n = std::make_shared<ossia::scene_node>();
  n->name = name;
  n->id.value = id;
  n->children
      = std::make_shared<std::vector<ossia::scene_payload>>(std::move(kids));
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
  return t;
}

ossia::material_component_ptr make_material(
    std::string tag, ossia::alpha_mode alpha = ossia::alpha_mode::opaque_,
    bool shadow_caster = true)
{
  auto m = std::make_shared<ossia::material_component>();
  m->tag = std::move(tag);
  m->alpha = alpha;
  m->shadow_caster = shadow_caster;
  return m;
}

ossia::mesh_component_ptr make_mesh(ossia::material_component_ptr mat)
{
  auto mesh = std::make_shared<ossia::mesh_component>();
  ossia::mesh_primitive prim;
  prim.material = std::move(mat);
  mesh->primitives.push_back(std::move(prim));
  return mesh;
}

//! Names of the scene_node children of `n`, in order.
std::vector<std::string> child_node_names(const ossia::scene_node& n)
{
  std::vector<std::string> out;
  if(n.children)
    for(const auto& p : *n.children)
      if(auto* c = ossia::get_if<ossia::scene_node_ptr>(&p); c && *c)
        out.push_back((*c)->name);
  return out;
}

std::vector<std::string> root_names(const ossia::scene_state& s)
{
  std::vector<std::string> out;
  if(s.roots)
    for(const auto& r : *s.roots)
      if(r)
        out.push_back(r->name);
  return out;
}

const ossia::scene_node*
child_node(const ossia::scene_node& n, std::string_view name)
{
  if(!n.children)
    return nullptr;
  for(const auto& p : *n.children)
    if(auto* c = ossia::get_if<ossia::scene_node_ptr>(&p); c && *c)
      if((*c)->name == name)
        return c->get();
  return nullptr;
}

bool has_transform(const ossia::scene_node& n)
{
  if(!n.children)
    return false;
  for(const auto& p : *n.children)
    if(ossia::get_if<ossia::scene_transform>(&p))
      return true;
  return false;
}
} // namespace

// ================================================================ PassThrough

TEST_CASE("SceneGraphFilter PassThrough republishes the input by identity",
          "[threedim][scene][filter]")
{
  Filter n;
  n.inputs.mode.value = Filter::PassThrough;
  auto in = make_state({make_node("a", 1)}, 3);
  n.inputs.scene_in.scene.state = in;

  n();
  CHECK(n.outputs.scene_out.scene.state.get() == in.get()); // no clone at all
  CHECK(n.outputs.scene_out.dirty == 0xFF);

  n();
  CHECK(n.outputs.scene_out.scene.state.get() == in.get());
  CHECK(n.outputs.scene_out.dirty == 0);
}

// ====================================================================== ByName

TEST_CASE("SceneGraphFilter ByName keeps matching nodes and drops the rest",
          "[threedim][scene][filter]")
{
  Filter n;
  auto wheel = make_node("Wheel", 2);
  auto door = make_node("Door", 3);
  auto car = make_node(
      "Car", 1,
      {trs(1.f, 2.f, 3.f), ossia::scene_node_ptr{wheel},
       ossia::scene_node_ptr{door}});
  auto in = make_state({car}, 7);
  auto mat = std::make_shared<ossia::material_component>();
  in->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{mat});
  in->active_camera_id.value = 42;
  in->dirty_index = 4;

  n.inputs.scene_in.scene.state = in;
  n.inputs.mode.value = Filter::ByName;
  n.inputs.names.value = {"Car", "Wheel"};

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  CHECK(st.get() != in.get());
  CHECK(st->version == 1); // the node's own monotone counter, not the input's
  CHECK(n.outputs.scene_out.dirty == 0xFF);

  REQUIRE(st->roots);
  REQUIRE(st->roots->size() == 1);
  const auto& out_car = (*st->roots)[0];
  CHECK(out_car.get() != car.get()); // a child was dropped -> clone
  CHECK(out_car->name == "Car");
  CHECK(out_car->dirty_index == car->dirty_index + 1);
  CHECK(child_node_names(*out_car) == std::vector<std::string>{"Wheel"});

  // The kept subtree is the ORIGINAL pointer, not a copy.
  const auto* kept = child_node(*out_car, "Wheel");
  REQUIRE(kept);
  CHECK(kept == wheel.get());

  // The matching node keeps its non-node payloads (the transform).
  CHECK(has_transform(*out_car));

  // Non-root scene fields ride through by identity; input stays untouched.
  CHECK(st->materials.get() == in->materials.get());
  CHECK(st->active_camera_id.value == 42);
  CHECK(st->dirty_index == in->dirty_index + 1);
  CHECK(car->children->size() == 3);
}

TEST_CASE("SceneGraphFilter keeps a non-matching ancestor as a wrapper for "
          "matching descendants",
          "[threedim][scene][filter]")
{
  // Car does not match; Wheels (and its child) do. The wrapper survives so
  // the match stays reachable, its dropped sibling Body disappears, and —
  // per the documented "payloads follow the node they're on" contract in
  // Walker::rewrite — the non-matching wrapper's own transform is dropped.
  Filter n;
  auto fl = make_node("FL", 3);
  auto wheels = make_node("Wheels", 2, {ossia::scene_node_ptr{fl}});
  auto body = make_node("Body", 4, {ossia::scene_node_ptr{make_node("Hood", 5)}});
  auto car = make_node(
      "Car", 1,
      {trs(1.f, 0.f, 0.f), ossia::scene_node_ptr{wheels},
       ossia::scene_node_ptr{body}});
  n.inputs.scene_in.scene.state = make_state({car});
  n.inputs.mode.value = Filter::ByPath;
  n.inputs.paths.value = {"/Car/Wheels", "/Car/Wheels/**"};

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots->size() == 1);
  const auto& out_car = (*st->roots)[0];
  CHECK(out_car.get() != car.get());
  CHECK(out_car->name == "Car");
  CHECK(child_node_names(*out_car) == std::vector<std::string>{"Wheels"});
  CHECK_FALSE(has_transform(*out_car)); // documented payload-follows-node drop

  // The fully-matching Wheels subtree is shared by identity, child included.
  const auto* out_wheels = child_node(*out_car, "Wheels");
  REQUIRE(out_wheels);
  CHECK(out_wheels == wheels.get());
  CHECK(child_node(*out_wheels, "FL") == fl.get());
}

// ================================================================ ByPath glob

TEST_CASE("SceneGraphFilter ByPath: ** crosses slashes, * does not",
          "[threedim][scene][filter]")
{
  Filter n;
  auto fl = make_node("FL", 3);
  auto wheels = make_node("Wheels", 2, {ossia::scene_node_ptr{fl}});
  auto car = make_node("Car", 1, {ossia::scene_node_ptr{wheels}});
  n.inputs.scene_in.scene.state = make_state({car});
  n.inputs.mode.value = Filter::ByPath;

  SECTION("/Car/** keeps every descendant and shares the whole tree")
  {
    n.inputs.paths.value = {"/Car/**"};
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    REQUIRE(st->roots->size() == 1);
    // Nothing was dropped or rewritten anywhere below the root, so the
    // share-if-unchanged path returns the ORIGINAL root pointer.
    CHECK((*st->roots)[0].get() == car.get());
    CHECK(n.outputs.scene_out.dirty == 0xFF); // first tick is still a change
  }

  SECTION("/Car/* stops at the first level: the grandchild is culled")
  {
    n.inputs.paths.value = {"/Car/*"};
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    REQUIRE(st->roots->size() == 1);
    const auto& out_car = (*st->roots)[0];
    CHECK(out_car.get() != car.get());
    const auto* out_wheels = child_node(*out_car, "Wheels");
    REQUIRE(out_wheels);
    CHECK(out_wheels != wheels.get()); // its child was dropped -> clone
    CHECK(child_node_names(*out_wheels).empty());
    // Original untouched.
    CHECK(child_node(*wheels, "FL") == fl.get());
  }
}

// ====================================================================== Invert

TEST_CASE("SceneGraphFilter Invert turns the list into an exclude filter",
          "[threedim][scene][filter]")
{
  Filter n;
  auto wheel = make_node("Wheel", 2);
  auto door = make_node("Door", 3);
  auto car = make_node(
      "Car", 1, {ossia::scene_node_ptr{wheel}, ossia::scene_node_ptr{door}});
  n.inputs.scene_in.scene.state = make_state({car});
  n.inputs.mode.value = Filter::ByName;
  n.inputs.names.value = {"Door"};
  n.inputs.invert.value = true;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots->size() == 1);
  const auto& out_car = (*st->roots)[0];
  CHECK(child_node_names(*out_car) == std::vector<std::string>{"Wheel"});
  CHECK(child_node(*out_car, "Wheel") == wheel.get()); // survivor by identity
}

// ================================================================= VisibleOnly

TEST_CASE("SceneGraphFilter VisibleOnly culls hidden leaves, keeps hidden "
          "wrappers of visible descendants",
          "[threedim][scene][filter]")
{
  // Predicates run per-node (header contract), so a hidden node whose child
  // is visible survives as a wrapper — the same wrapper-preservation rule
  // every mode uses. A hidden node with nothing visible below is dropped.
  Filter n;
  auto shown = make_node("Shown", 2);
  auto hidden_leaf = make_node("HiddenLeaf", 3);
  hidden_leaf->visible = false;
  auto kid = make_node("Kid", 5);
  auto hidden_wrap = make_node("HiddenWrap", 4, {ossia::scene_node_ptr{kid}});
  hidden_wrap->visible = false;
  auto root = make_node(
      "Root", 1,
      {ossia::scene_node_ptr{shown}, ossia::scene_node_ptr{hidden_leaf},
       ossia::scene_node_ptr{hidden_wrap}});
  n.inputs.scene_in.scene.state = make_state({root});
  n.inputs.mode.value = Filter::VisibleOnly;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots->size() == 1);
  const auto& out_root = (*st->roots)[0];
  CHECK(child_node_names(*out_root)
        == std::vector<std::string>{"Shown", "HiddenWrap"});
  CHECK(child_node(*out_root, "Shown") == shown.get()); // identity share
  const auto* out_wrap = child_node(*out_root, "HiddenWrap");
  REQUIRE(out_wrap);
  CHECK(child_node(*out_wrap, "Kid") == kid.get());
}

// =============================================== component / material / schema

TEST_CASE("SceneGraphFilter component, material and schema-field predicates",
          "[threedim][scene][filter]")
{
  Filter n;

  SECTION("ByComponent keeps nodes carrying the selected payload kind")
  {
    auto lit = make_node(
        "Lamp", 2,
        {ossia::light_component_ptr{std::make_shared<ossia::light_component>()}});
    auto meshy = make_node("Meshy", 3, {make_mesh(make_material("m"))});
    n.inputs.scene_in.scene.state = make_state({lit, meshy});
    n.inputs.mode.value = Filter::ByComponent;
    n.inputs.component.value = Filter::Light;

    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    CHECK(root_names(*n.outputs.scene_out.scene.state)
          == std::vector<std::string>{"Lamp"});
  }

  SECTION("ByMaterialTag globs against every primitive's material tag")
  {
    auto glass = make_node("GlassThing", 2, {make_mesh(make_material("glass"))});
    auto metal = make_node("MetalThing", 3, {make_mesh(make_material("metal"))});
    n.inputs.scene_in.scene.state = make_state({glass, metal});
    n.inputs.mode.value = Filter::ByMaterialTag;
    n.inputs.material_tags.value = {"gl*"};

    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    CHECK(root_names(*n.outputs.scene_out.scene.state)
          == std::vector<std::string>{"GlassThing"});
  }

  SECTION("ByAlphaMode selects on material.alpha")
  {
    auto blend = make_node(
        "Blend", 2, {make_mesh(make_material("a", ossia::alpha_mode::blend))});
    auto opaque = make_node(
        "Opaque", 3, {make_mesh(make_material("b", ossia::alpha_mode::opaque_))});
    n.inputs.scene_in.scene.state = make_state({blend, opaque});
    n.inputs.mode.value = Filter::ByAlphaMode;
    n.inputs.alpha_mode.value = Filter::AlphaBlend;

    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    CHECK(root_names(*n.outputs.scene_out.scene.state)
          == std::vector<std::string>{"Blend"});
  }

  SECTION("ByShadowCaster compares the flag against the Caster toggle")
  {
    auto caster = make_node(
        "Caster", 2,
        {make_mesh(make_material("a", ossia::alpha_mode::opaque_, true))});
    auto ghost = make_node(
        "Ghost", 3,
        {make_mesh(make_material("b", ossia::alpha_mode::opaque_, false))});
    n.inputs.scene_in.scene.state = make_state({caster, ghost});
    n.inputs.mode.value = Filter::ByShadowCaster;
    n.inputs.caster_flag.value = false;

    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    CHECK(root_names(*n.outputs.scene_out.scene.state)
          == std::vector<std::string>{"Ghost"});
  }

  SECTION("ByPurpose selects on scene_node::purpose")
  {
    auto guide = make_node("Gizmo", 2);
    guide->purpose = ossia::scene_purpose::guide;
    auto render = make_node("Hero", 3);
    render->purpose = ossia::scene_purpose::render;
    n.inputs.scene_in.scene.state = make_state({guide, render});
    n.inputs.mode.value = Filter::ByPurpose;
    n.inputs.purpose.value = Filter::PurposeGuide;

    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    CHECK(root_names(*n.outputs.scene_out.scene.state)
          == std::vector<std::string>{"Gizmo"});
  }
}

// ========================================================= property predicates

TEST_CASE("SceneGraphFilter property predicates: contains works, missing key "
          "never matches",
          "[threedim][scene][filter]")
{
  Filter n;

  SECTION("ByNodeProperty PropContains substring-matches the stored string")
  {
    auto hero = make_node("Hero", 2);
    hero->properties["role"] = std::string("hero-prop");
    auto villain = make_node("Villain", 3);
    villain->properties["role"] = std::string("villain");
    auto blank = make_node("Blank", 4); // no properties at all
    n.inputs.scene_in.scene.state = make_state({hero, villain, blank});
    n.inputs.mode.value = Filter::ByNodeProperty;
    n.inputs.prop_key.value = "role";
    n.inputs.prop_op.value = Filter::PropContains;
    n.inputs.prop_value.value = "hero";

    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    CHECK(root_names(*n.outputs.scene_out.scene.state)
          == std::vector<std::string>{"Hero"});
  }

  SECTION("ByMaterialProperty reads the primitive materials' property map")
  {
    auto tagged_mat = std::make_shared<ossia::material_component>();
    tagged_mat->properties["usd:kind"] = std::string("hero-asset");
    auto plain_mat = std::make_shared<ossia::material_component>();

    auto tagged = make_node("Tagged", 2, {make_mesh(tagged_mat)});
    auto plain = make_node("Plain", 3, {make_mesh(plain_mat)});
    n.inputs.scene_in.scene.state = make_state({tagged, plain});
    n.inputs.mode.value = Filter::ByMaterialProperty;
    n.inputs.prop_key.value = "usd:kind";
    n.inputs.prop_op.value = Filter::PropContains;
    n.inputs.prop_value.value = "hero";

    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    CHECK(root_names(*n.outputs.scene_out.scene.state)
          == std::vector<std::string>{"Tagged"});
  }

  SECTION("an absent key matches nothing: the output degrades to empty")
  {
    auto a = make_node("A", 2);
    n.inputs.scene_in.scene.state = make_state({a});
    n.inputs.mode.value = Filter::ByNodeProperty;
    n.inputs.prop_key.value = "no-such-key";
    n.inputs.prop_op.value = Filter::PropEqual;
    n.inputs.prop_value.value = "x";

    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st); // an empty state, not a null one
    CHECK(st->empty());
  }
}

// =============================================================== SetVisibility

TEST_CASE("SceneGraphFilter SetVisibility hides but never drops",
          "[threedim][scene][filter]")
{
  // "*" legitimately matches every node, so this pins the part of
  // SetVisibility that works regardless of the gating defect below.
  Filter n;
  auto wheel = make_node("Wheel", 2);
  auto car
      = make_node("Car", 1, {trs(1.f, 0.f, 0.f), ossia::scene_node_ptr{wheel}});
  n.inputs.scene_in.scene.state = make_state({car});
  n.inputs.mode.value = Filter::SetVisibility;
  n.inputs.names.value = {"*"};

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  CHECK(n.outputs.scene_out.dirty == 0xFF);
  REQUIRE(st->roots->size() == 1);
  const auto& out_car = (*st->roots)[0];
  CHECK_FALSE(out_car->visible);
  // The subtree is still whole: node AND payloads survive, just hidden.
  CHECK(has_transform(*out_car));
  const auto* out_wheel = child_node(*out_car, "Wheel");
  REQUIRE(out_wheel);
  CHECK_FALSE(out_wheel->visible);
  // Upstream untouched.
  CHECK(car->visible);
  CHECK(wheel->visible);
}

TEST_CASE("SceneGraphFilter SetVisibility must only touch nodes matching Names",
          "[threedim][scene][filter][!shouldfail]")
{
  // DEFECT: the Names gate is never applied in SetVisibility mode.
  // node_matches() returns true for every node in this mode, with a comment
  // claiming "the real gating happens at the caller level using name-list
  // matching" — but Walker::rewrite never consults ctx.names, so EVERY node
  // is hidden, not just the listed ones. The header documents: "matching
  // nodes have their `visible` flag flipped ... Non-matching nodes kept
  // untouched."
  Filter n;
  auto wheel = make_node("Wheel", 2);
  auto door = make_node("Door", 3);
  auto car = make_node(
      "Car", 1, {ossia::scene_node_ptr{wheel}, ossia::scene_node_ptr{door}});
  n.inputs.scene_in.scene.state = make_state({car});
  n.inputs.mode.value = Filter::SetVisibility;
  n.inputs.names.value = {"Wheel"};

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots->size() == 1);
  const auto& out_car = (*st->roots)[0];
  const auto* out_wheel = child_node(*out_car, "Wheel");
  const auto* out_door = child_node(*out_car, "Door");
  REQUIRE(out_wheel);
  REQUIRE(out_door);
  CHECK_FALSE(out_wheel->visible); // the listed node IS hidden (works today)
  CHECK(out_door->visible);        // FAILS: unlisted node hidden too
  CHECK(out_car->visible);         // FAILS: unlisted root hidden too
}

TEST_CASE("SceneGraphFilter ByNodeProperty must compare the stored value, not "
          "its debug string",
          "[threedim][scene][filter][!shouldfail]")
{
  // DEFECT: the property predicate stringifies the stored ossia::value with
  // value_to_pretty_string(), which is fmt::format("{}", v) and prints a
  // TYPED DEBUG string: int 3 -> "int: 3", float 0.4f -> "float: 0.40",
  // "x" -> "string: \"x\"".  Consequences:
  //  - PropEqual against a user literal like "3" can never match;
  //  - PropLessThan/GreaterThan: std::stod("float: 0.40") throws, so the
  //    lexicographic fallback compares "float: 0.40" with "0.5" and
  //    'f' > '0' makes GreaterThan spuriously true for every float value.
  // The fix is to compare against the bare value (or convert the stored
  // value directly), after which both sections go green.
  Filter n;

  SECTION("PropEqual on an int property must match its literal text")
  {
    auto a = make_node("A", 2);
    a->properties["layer"] = 3;
    auto b = make_node("B", 3);
    n.inputs.scene_in.scene.state = make_state({a, b});
    n.inputs.mode.value = Filter::ByNodeProperty;
    n.inputs.prop_key.value = "layer";
    n.inputs.prop_op.value = Filter::PropEqual;
    n.inputs.prop_value.value = "3";

    n();
    REQUIRE(n.outputs.scene_out.scene.state);
    // FAILS: "int: 3" != "3" -> nothing matches -> empty output.
    CHECK(root_names(*n.outputs.scene_out.scene.state)
          == std::vector<std::string>{"A"});
  }

  SECTION("PropGreaterThan must compare numerically, not lexicographically")
  {
    auto a = make_node("A", 2);
    a->properties["cutoff"] = 0.4f;
    n.inputs.scene_in.scene.state = make_state({a});
    n.inputs.mode.value = Filter::ByNodeProperty;
    n.inputs.prop_key.value = "cutoff";
    n.inputs.prop_op.value = Filter::PropGreaterThan;
    n.inputs.prop_value.value = "0.5";

    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    // 0.4 > 0.5 is false, so A must be dropped.
    // FAILS: stod("float: 0.40") throws -> lexicographic 'f' > '0' -> kept.
    CHECK(st->empty());
  }
}

// ============================================================ tick discipline

TEST_CASE("SceneGraphFilter rebuilds only on a real change",
          "[threedim][scene][filter]")
{
  Filter n;
  auto wheel = make_node("Wheel", 2);
  auto car = make_node("Car", 1, {ossia::scene_node_ptr{wheel}});
  auto in = make_state({car}, 5);
  n.inputs.scene_in.scene.state = in;
  n.inputs.mode.value = Filter::ByName;
  n.inputs.names.value = {"*"};

  n();
  const auto first = n.outputs.scene_out.scene.state;
  REQUIRE(first);
  CHECK(first->version == 1);
  CHECK(n.outputs.scene_out.dirty == 0xFF);
  // All nodes match, nothing rewritten: the root rides through by identity
  // even though the state wrapper is new.
  CHECK((*first->roots)[0].get() == car.get());

  // An idle tick republishes the same object: no re-bump, no re-dirty.
  n();
  CHECK(n.outputs.scene_out.scene.state == first);
  CHECK(n.outputs.scene_out.scene.state->version == first->version);
  CHECK(n.outputs.scene_out.dirty == 0);

  SECTION("an upstream version bump on the same pointer rebuilds")
  {
    in->version = 6;
    n();
    CHECK(n.outputs.scene_out.scene.state != first);
    CHECK(n.outputs.scene_out.scene.state->version == first->version + 1);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    n();
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("a new upstream pointer with the SAME version rebuilds")
  {
    n.inputs.scene_in.scene.state = make_state({make_node("Other", 9)}, 5);
    n();
    CHECK(n.outputs.scene_out.scene.state != first);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
  }

  SECTION("a control change rebuilds and bumps once")
  {
    n.inputs.names.value = {"Nope"};
    n.rebuild(); // what the halp update() hook does on a control edit
    n();
    CHECK(n.outputs.scene_out.scene.state != first);
    CHECK(n.outputs.scene_out.scene.state->version == first->version + 1);
    CHECK(n.outputs.scene_out.scene.state->empty());
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    n();
    CHECK(n.outputs.scene_out.dirty == 0);
  }
}

// ============================================================== empty / null

TEST_CASE("SceneGraphFilter degrades gracefully on empty input",
          "[threedim][scene][filter]")
{
  Filter n;
  n.inputs.mode.value = Filter::ByName;
  n.inputs.names.value = {"*"};

  SECTION("a rootless input filters to a valid empty state, not null")
  {
    n.inputs.scene_in.scene.state = make_state({});
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    CHECK(st->empty());
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    n();
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("no-match input yields an empty state and settles quietly")
  {
    n.inputs.scene_in.scene.state = make_state({make_node("a", 1)});
    n.inputs.names.value = {"Zzz"};
    n();
    const auto& st = n.outputs.scene_out.scene.state;
    REQUIRE(st);
    CHECK(st->empty());
    n();
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("an input that goes null clears the output without a dirty pulse")
  {
    n.inputs.scene_in.scene.state = make_state({make_node("a", 1)});
    n();
    REQUIRE(n.outputs.scene_out.scene.state);

    n.inputs.scene_in.scene.state = nullptr;
    n();
    CHECK(n.outputs.scene_out.scene.state == nullptr);
    CHECK(n.outputs.scene_out.dirty == 0);
  }
}

TEST_CASE("SceneGraphFilter never-wired must stay quiet from the first tick",
          "[threedim][scene][filter][!shouldfail]")
{
  // DEFECT: m_pending_dirty is default-initialised to 0xFF, so the very
  // first tick of a node with NO upstream scene emits dirty == 0xFF with a
  // null state. Same contract as the TagAs / Transform3D null-upstream pins
  // in SceneGraphOps.cpp: no scene in, no scene out, nothing dirty — a 0xFF
  // here invalidates every identity-keyed preprocessor cache downstream of
  // an unwired node.
  Filter n;
  n.inputs.mode.value = Filter::ByName;
  n();
  CHECK(n.outputs.scene_out.scene.state == nullptr);
  CHECK(n.outputs.scene_out.dirty == 0); // FAILS: 0xFF leaks from the init
}

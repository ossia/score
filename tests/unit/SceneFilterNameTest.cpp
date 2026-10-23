// Scene Filter "keep matching names" (shader board P03): the subtrees whose
// node name matches an OSC-style pattern -- libossia's matcher: `*`, `?`,
// `[0-9]`, `{a,b}` -- are kept whole, with their parent chain so they stay
// where they were: a parent kept only for a match below it keeps its
// transforms and drops its own components. Pure CPU.
#include <Gfx/Graph/SceneFilterNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

namespace
{
using payloads = std::vector<ossia::scene_payload>;

ossia::scene_node_ptr node(std::string name, payloads children = {})
{
  auto n = std::make_shared<ossia::scene_node>();
  n->name = std::move(name);
  n->children = std::make_shared<const payloads>(std::move(children));
  return n;
}

ossia::scene_payload mesh()
{
  return std::make_shared<const ossia::mesh_component>();
}

ossia::scene_payload transform()
{
  ossia::scene_transform t;
  t.translation[0] = 1.f;
  return t;
}

// World { transform, mesh, Car { transform, mesh, Wheel_FL { mesh },
// Wheel_FR { mesh } }, Tree { mesh } }
ossia::scene_spec scene()
{
  auto st = std::make_shared<ossia::scene_state>();
  auto car = node(
      "Car", {transform(), mesh(), node("Wheel_FL", {mesh()}), node("Wheel_FR", {mesh()})});
  auto world = node("World", {transform(), mesh(), car, node("Tree", {mesh()})});
  st->roots = std::make_shared<const std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{world});
  ossia::scene_spec s;
  s.state = st;
  return s;
}

// The tree as text: name(child names / t for a transform / m for a mesh).
std::string dump(const ossia::scene_node& n)
{
  std::string s = n.name + "(";
  bool first = true;
  if(n.children)
    for(const auto& c : *n.children)
    {
      if(!first)
        s += " ";
      first = false;
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&c))
        s += dump(**sub);
      else if(ossia::get_if<ossia::scene_transform>(&c))
        s += "t";
      else if(ossia::get_if<ossia::mesh_component_ptr>(&c))
        s += "m";
      else
        s += "?";
    }
  return s + ")";
}

std::string dump(const ossia::scene_spec& s)
{
  std::string out;
  if(s.state && s.state->roots)
    for(const auto& r : *s.state->roots)
      out += dump(*r);
  return out;
}
}

TEST_CASE("Scene Filter keeps the subtrees whose name matches", "[gfx][scene][P03]")
{
  const auto in = scene();
  using score::gfx::filterScene;
  CHECK(dump(filterScene(in, 2, "Wheel_F?")) == "World(t Car(t Wheel_FL(m) Wheel_FR(m)))");
  CHECK(dump(filterScene(in, 2, "Wheel_FL")) == "World(t Car(t Wheel_FL(m)))");
  CHECK(dump(filterScene(in, 2, "Tree")) == "World(t Tree(m))");
  CHECK(dump(filterScene(in, 2, "{Car,Tree}"))
        == "World(t Car(t m Wheel_FL(m) Wheel_FR(m)) Tree(m))");
  CHECK(dump(filterScene(in, 2, "*")) == dump(in));
  // A root that matches is kept whole.
  CHECK(dump(filterScene(in, 2, "World")) == dump(in));
  CHECK(dump(filterScene(in, 2, "Nothing")).empty());
  // The pattern matches the whole name, not a part of it.
  CHECK(dump(filterScene(in, 2, "Wheel")).empty());
  // No pattern: nothing to select, the scene passes.
  CHECK(dump(filterScene(in, 2, "")) == dump(in));
}

TEST_CASE("Scene Filter modes 0 and 1 are unchanged", "[gfx][scene][P03]")
{
  const auto in = scene();
  CHECK(score::gfx::filterScene(in, 0, "Tree").state == in.state);
  CHECK(dump(score::gfx::filterScene(in, 1, "")) == dump(in));
}

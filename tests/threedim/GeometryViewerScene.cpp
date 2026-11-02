// What the Geometry viewer takes from a scene before drawing it: the draws,
// the world bounds it frames, the gizmos of lights and cameras, and the
// summary its views show. GPU-free: describeScene is the walk the viewer's
// renderer does when the scene changes.

#include <Threedim/GeometryViewer/SceneSummary.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <vector>

using namespace Gfx::GeometryViewer;
using Catch::Approx;

namespace
{
//! A triangle with positions in memory, bounds left for the viewer to find
ossia::mesh_component_ptr triangle(ossia::material_component_ptr mat = {})
{
  static const float positions[9]{0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
  auto data = std::make_shared<ossia::buffer_resource>();
  auto bytes = std::shared_ptr<float[]>(new float[9]);
  std::memcpy(bytes.get(), positions, sizeof(positions));
  data->resource = ossia::buffer_data{std::shared_ptr<const void>(bytes, bytes.get()), sizeof(positions)};

  ossia::mesh_primitive p;
  p.vertex_buffers.push_back(data);
  p.attributes.push_back(
      {.semantic = ossia::attribute_semantic::position,
       .format = ossia::vertex_format::float3, .buffer_index = 0, .byte_offset = 0,
       .byte_stride = 12});
  p.vertex_count = 3;
  p.material = std::move(mat);

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(p));
  return mesh;
}

ossia::scene_transform translate(float x, float y, float z)
{
  ossia::scene_transform t;
  t.translation[0] = x;
  t.translation[1] = y;
  t.translation[2] = z;
  return t;
}

ossia::scene_node_ptr node(std::string name, std::vector<ossia::scene_payload> children)
{
  auto n = std::make_shared<ossia::scene_node>();
  n->name = std::move(name);
  n->children = std::make_shared<const std::vector<ossia::scene_payload>>(std::move(children));
  return n;
}

ossia::scene_spec scene(std::vector<ossia::scene_node_ptr> roots)
{
  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::make_shared<const std::vector<ossia::scene_node_ptr>>(std::move(roots));
  return ossia::scene_spec{std::move(state)};
}
}

TEST_CASE("Two meshes at different places: the bounds cover both", "[threedim][viewer]")
{
  auto mat = std::make_shared<ossia::material_component>();
  mat->tag = "Red";
  const auto s = scene({node(
      "Root", {node("Left", {translate(-5.f, 0.f, 0.f), triangle(mat)}),
               node("Right", {translate(5.f, 2.f, 0.f), triangle()})})});

  SceneDescription d;
  describeScene(s, d);

  REQUIRE(d.draws.size() == 2);
  CHECK(d.draws[0].world.map(QVector3D{}).x() == Approx(-5.f));
  CHECK(d.draws[1].world.map(QVector3D{}).x() == Approx(5.f));
  CHECK(d.draws[0].material == mat);

  REQUIRE(!d.bounds.empty());
  CHECK(d.bounds.min[0] == Approx(-5.f));
  CHECK(d.bounds.max[0] == Approx(6.f));
  CHECK(d.bounds.min[1] == Approx(0.f));
  CHECK(d.bounds.max[1] == Approx(3.f));

  const auto& c = d.summary.counts;
  CHECK(c.nodes == 3);
  CHECK(c.meshes == 2);
  CHECK(c.primitives == 2);
  CHECK(c.vertices == 6);
  CHECK(c.materials == 1);

  // Root, Left, its mesh, Right, its mesh
  REQUIRE(d.summary.rows.size() == 5);
  CHECK(d.summary.rows[0].name == "Root");
  CHECK(d.summary.rows[2].kind == SceneRow::Kind::Mesh);
  CHECK(d.summary.rows[2].parent == 1);
  CHECK(d.summary.rows[2].details.find("material Red") != std::string::npos);
  // A node is where its objects are
  CHECK(d.summary.rows[1].bounds.max[0] == Approx(-4.f));
  CHECK(d.summary.rows[0].bounds.max[0] == Approx(6.f));
}

TEST_CASE("A transform only moves the siblings after it", "[threedim][viewer]")
{
  const auto s = scene({node("Root", {triangle(), translate(0.f, 10.f, 0.f), triangle()})});
  SceneDescription d;
  describeScene(s, d);
  REQUIRE(d.draws.size() == 2);
  CHECK(d.draws[0].world.isIdentity());
  CHECK(d.draws[1].world.map(QVector3D{}).y() == Approx(10.f));
  CHECK(d.bounds.max[1] == Approx(11.f));
}

TEST_CASE("Lights and cameras become gizmos, out of the framed bounds", "[threedim][viewer]")
{
  auto light = std::make_shared<ossia::light_component>();
  light->type = ossia::light_type::spot;
  light->color[0] = 1.f;
  light->color[1] = 0.5f;
  light->color[2] = 0.f;
  auto camera = std::make_shared<ossia::camera_component>();
  camera->yfov = 0.8f;

  const auto s = scene({node(
      "Root", {triangle(), node("Lamp", {translate(0.f, 50.f, 0.f), light}),
               node("Eye", {translate(0.f, 0.f, 30.f), camera})})});
  SceneDescription d;
  describeScene(s, d);

  CHECK(d.summary.counts.lights == 1);
  CHECK(d.summary.counts.cameras == 1);
  REQUIRE(d.gizmos.size() == 2);
  CHECK(d.gizmos[0].kind == SceneGizmo::Light);
  CHECK(d.gizmos[0].color[1] == Approx(0.5f));
  CHECK(d.gizmos[0].world.map(QVector3D{}).y() == Approx(50.f));
  CHECK(d.gizmos[1].kind == SceneGizmo::Camera);
  CHECK(d.gizmos[1].yfov == Approx(0.8f));
  // Their rows have a position for the inspector's outline
  CHECK(d.summary.rows[d.gizmos[1].row].bounds.min[2] == Approx(30.f));
  // What is framed is what is drawn
  CHECK(d.bounds.max[1] == Approx(1.f));
}

TEST_CASE("Instances held in memory are drawn one by one", "[threedim][viewer]")
{
  auto inst = std::make_shared<ossia::instance_component>();
  inst->prototype = triangle();
  inst->transform_type = ossia::instance_component::transform_format::translation;
  inst->instance_count = 3;
  auto offsets = std::shared_ptr<float[]>(new float[9]{0, 0, 0, 10, 0, 0, 20, 0, 0});
  auto res = std::make_shared<ossia::buffer_resource>();
  res->resource = ossia::buffer_data{std::shared_ptr<const void>(offsets, offsets.get()), 36};
  inst->instance_transforms = res;

  const auto s = scene({node("Root", {ossia::instance_component_ptr(inst)})});
  SceneDescription d;
  describeScene(s, d);
  REQUIRE(d.draws.size() == 3);
  CHECK(d.draws[2].world.map(QVector3D{}).x() == Approx(20.f));
  CHECK(d.bounds.max[0] == Approx(21.f));
  CHECK(d.summary.counts.instances == 3);
  CHECK(d.skipped_instances == 0);
}

TEST_CASE("A plain geometry is a single draw where it is", "[threedim][viewer]")
{
  auto list = std::make_shared<ossia::mesh_list>();
  list->meshes.emplace_back().vertices = 3;
  const auto s = ossia::wrap_geometry_as_scene(
      ossia::geometry_spec{list, std::make_shared<ossia::geometry_filter_list>()});
  SceneDescription d;
  describeScene(s, d);
  REQUIRE(d.draws.size() == 1);
  CHECK(d.draws[0].legacy != nullptr);
  CHECK(d.draws[0].world.isIdentity());
  CHECK(d.gizmos.empty());
}

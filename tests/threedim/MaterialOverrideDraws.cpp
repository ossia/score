// Threedim::MaterialOverride reaches the draws.
//
// Primitives reference their material by pointer and flattenScene resolves a
// draw's material_index by looking that pointer up in scene_state.materials.
// Swapping the materials table for clones alone leaves every draw on the
// source material, which is no longer in the table: the override then has no
// visible effect. The meshes using a targeted material must point at its
// clone.

#include <Gfx/Graph/SceneGPUState.hpp>
#include <Threedim/MaterialOverride.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using Catch::Approx;

namespace
{
std::shared_ptr<ossia::material_component> make_material(float r)
{
  auto m = std::make_shared<ossia::material_component>();
  m->base_color_factor[0] = r;
  m->base_color_factor[1] = 0.f;
  m->base_color_factor[2] = 0.f;
  m->base_color_factor[3] = 1.f;
  return m;
}

ossia::scene_node_ptr
mesh_node(uint64_t id, const ossia::material_component_ptr& mat)
{
  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(std::make_shared<ossia::buffer_resource>());
  prim.vertex_count = 3;
  prim.material = mat;
  prim.stable_id = id;

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));

  auto n = std::make_shared<ossia::scene_node>();
  n->id.value = id;
  n->children = std::make_shared<std::vector<ossia::scene_payload>>(
      std::vector<ossia::scene_payload>{ossia::mesh_component_ptr(mesh)});
  return n;
}

//! The flattened base colour red channel of each draw, in draw order.
std::vector<float> drawn_red(const std::shared_ptr<const ossia::scene_state>& s)
{
  ossia::scene_spec spec;
  spec.state = s;
  score::gfx::FlatScene flat;
  score::gfx::flattenScene(spec, flat, 1.f);
  std::vector<float> res;
  for(const auto& dc : flat.draws)
  {
    REQUIRE(dc.materialIndex >= 0);
    REQUIRE(dc.materialIndex < (int)flat.materials.size());
    res.push_back(flat.materials[dc.materialIndex].baseColor[0]);
  }
  return res;
}
}

TEST_CASE("Material Override changes the material the draws use",
          "[threedim][material_override]")
{
  auto m0 = make_material(0.1f);
  auto m1 = make_material(0.3f);
  auto in = std::make_shared<ossia::scene_state>();
  auto n0 = mesh_node(1, m0);
  auto n1 = mesh_node(2, m1);
  in->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{n0, n1});
  in->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{m0, m1});
  in->version = 1;

  REQUIRE(drawn_red(in) == std::vector<float>{0.1f, 0.3f});

  Threedim::MaterialOverride n;
  n.inputs.scene_in.scene.state = in;
  n.inputs.use_base_color.value = true;
  n.inputs.base_color.value.r = 0.9f;

  SECTION("All mode: every draw takes the override")
  {
    n();
    const auto out = n.outputs.scene_out.scene.state;
    REQUIRE(out);
    const auto red = drawn_red(out);
    REQUIRE(red.size() == 2);
    CHECK(red[0] == Approx(0.9f));
    CHECK(red[1] == Approx(0.9f));

    // The input tree is untouched.
    CHECK(drawn_red(in) == std::vector<float>{0.1f, 0.3f});
  }

  SECTION("By index: only the draws of that material change; the rest of "
          "the tree passes through by identity")
  {
    n.inputs.mode.value = Threedim::MaterialOverride::ByIndex;
    n.inputs.index.value = 1;
    n();
    const auto out = n.outputs.scene_out.scene.state;
    REQUIRE(out);
    const auto red = drawn_red(out);
    REQUIRE(red.size() == 2);
    CHECK(red[0] == Approx(0.1f));
    CHECK(red[1] == Approx(0.9f));
    REQUIRE(out->roots);
    CHECK((*out->roots)[0] == n0);
    CHECK((*out->roots)[1] != n1);
  }

  SECTION("the rewritten meshes are reused while upstream is unchanged")
  {
    n();
    const auto first = n.outputs.scene_out.scene.state;
    const auto mesh_of = [](const ossia::scene_state& s) {
      return ossia::get<ossia::mesh_component_ptr>((*(*s.roots)[0]->children)[0]);
    };
    n.inputs.base_color.value.r = 0.5f;
    n.rebuild();
    n();
    const auto second = n.outputs.scene_out.scene.state;
    REQUIRE(second != first);
    CHECK(mesh_of(*second) == mesh_of(*first));
    CHECK(drawn_red(second)[0] == Approx(0.5f));
  }
}

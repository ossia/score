// Threedim::MaterialOverride overrides a material's colours from its colour
// controls: the RGBA "Color" chooser becomes base_color_factor (alpha
// included), the "Emissive" chooser becomes emissive_factor and "Emissive
// strength" the emissive_strength multiplier. A colour change reaches the
// output through the control's update() callback, as the engine calls it.
// Pure logic, no GPU.

#include <Threedim/MaterialOverride.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using Catch::Approx;

TEST_CASE(
    "Material Override publishes base colour, alpha and emissive from its "
    "colour controls",
    "[threedim][material_override]")
{
  auto src = std::make_shared<ossia::material_component>();
  src->base_color_factor[0] = 0.9f;
  auto in = std::make_shared<ossia::scene_state>();
  in->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  in->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{src});
  in->version = 1;

  Threedim::MaterialOverride n;
  n.inputs.scene_in.scene.state = in;
  n.inputs.use_base_color.value = true;
  n.inputs.base_color.value = {0.2f, 0.4f, 0.6f, 0.5f};
  n.inputs.use_emissive.value = true;
  n.inputs.emissive.value = {0.1f, 0.3f, 1.0f, 1.0f};
  n.inputs.em_strength.value = 50.f;
  n();

  const auto st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->materials);
  REQUIRE(st->materials->size() == 1);
  const auto& mat = *(*st->materials)[0];

  CHECK(mat.base_color_factor[0] == Approx(0.2f));
  CHECK(mat.base_color_factor[1] == Approx(0.4f));
  CHECK(mat.base_color_factor[2] == Approx(0.6f));
  CHECK(mat.base_color_factor[3] == Approx(0.5f));

  // The shader emits emissive_factor * emissive_strength. Strength has the
  // range of PBR Mesh's, so the brightest emission of the former 0..10
  // per-channel sliders is still reached.
  CHECK(decltype(n.inputs.em_strength)::range{}.max == Approx(100.));
  CHECK(mat.emissive_factor[0] * mat.emissive_strength == Approx(5.f));
  CHECK(mat.emissive_factor[1] * mat.emissive_strength == Approx(15.f));
  CHECK(mat.emissive_factor[2] * mat.emissive_strength == Approx(50.f));

  SECTION("a base colour change republishes the material")
  {
    n.inputs.base_color.value.a = 1.f;
    n.inputs.base_color.update(n);
    n();
    const auto st2 = n.outputs.scene_out.scene.state;
    REQUIRE(st2);
    REQUIRE(st2 != st);
    CHECK((*st2->materials)[0]->base_color_factor[3] == Approx(1.f));
  }

  SECTION("an emissive colour change republishes the material")
  {
    n.inputs.emissive.value.g = 0.f;
    n.inputs.emissive.update(n);
    n();
    const auto st2 = n.outputs.scene_out.scene.state;
    REQUIRE(st2);
    REQUIRE(st2 != st);
    const auto& mat2 = *(*st2->materials)[0];
    CHECK(mat2.emissive_factor[1] * mat2.emissive_strength == Approx(0.f));
  }
}

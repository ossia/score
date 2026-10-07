// Threedim::PBRMesh publishes its material from the colour controls: the
// RGBA "Color" chooser becomes base_color_factor (alpha included), the
// "Emissive" chooser becomes emissive_factor and "Emissive strength" the
// emissive_strength multiplier. Pure logic, no GPU: the mesh buffer handle
// is a dummy pointer that operator()() only copies.

#include <Threedim/PBRMesh.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

using Catch::Approx;

TEST_CASE(
    "PBR Mesh publishes base colour, alpha and emissive from its colour controls",
    "[threedim][pbrmesh]")
{
  Threedim::PBRMesh node;

  int dummy_buffer = 0;
  auto& mesh = node.inputs.geometry_in.mesh;
  mesh.buffers.push_back({.handle = &dummy_buffer, .byte_size = 3 * 12});
  mesh.vertices = 3;

  node.inputs.base_color.value = {0.2f, 0.4f, 0.6f, 0.5f};
  node.inputs.emissive.value = {0.1f, 0.3f, 1.0f, 1.0f};
  node.inputs.em_strength.value = 5.f;
  node.inputs.metallic.value = 0.7f;
  node.inputs.roughness.value = 0.25f;
  node();

  const auto st = node.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->materials);
  REQUIRE(st->materials->size() == 1);
  const auto& mat = *(*st->materials)[0];

  CHECK(mat.base_color_factor[0] == Approx(0.2f));
  CHECK(mat.base_color_factor[1] == Approx(0.4f));
  CHECK(mat.base_color_factor[2] == Approx(0.6f));
  CHECK(mat.base_color_factor[3] == Approx(0.5f));
  CHECK(mat.metallic_factor == Approx(0.7f));
  CHECK(mat.roughness_factor == Approx(0.25f));

  // The shader emits emissive_factor * emissive_strength.
  CHECK(mat.emissive_factor[0] * mat.emissive_strength == Approx(0.5f));
  CHECK(mat.emissive_factor[1] * mat.emissive_strength == Approx(1.5f));
  CHECK(mat.emissive_factor[2] * mat.emissive_strength == Approx(5.0f));

  SECTION("a colour change republishes the material")
  {
    node.inputs.base_color.value.a = 1.f;
    node.inputs.emissive.value.g = 0.f;
    node();
    const auto st2 = node.outputs.scene_out.scene.state;
    REQUIRE(st2);
    REQUIRE(st2 != st);
    const auto& mat2 = *(*st2->materials)[0];
    CHECK(mat2.base_color_factor[3] == Approx(1.f));
    CHECK(mat2.emissive_factor[1] * mat2.emissive_strength == Approx(0.f));
  }
}

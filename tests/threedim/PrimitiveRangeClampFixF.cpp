// Pins that the primitive generators clamp their subdivision inputs to the
// declared ranges. Values set from JS / OSC bypass the slider range; the
// icosphere is exponential in its subdivision level, so Sphere Subdivisions 32
// used to hang the renderer.

#include <Threedim/Primitive.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Sphere clamps Subdivisions to 1..5", "[threedim][primitive]")
{
  Threedim::Sphere ref;
  ref.inputs.subdiv.value = 5;
  ref.update();
  const auto max_vertices = ref.outputs.geometry.mesh.vertices;
  REQUIRE(max_vertices == 60 * 1024);

  Threedim::Sphere big;
  big.inputs.subdiv.value = 7;
  big.update();
  CHECK(big.outputs.geometry.mesh.vertices == max_vertices);

  Threedim::Sphere neg;
  neg.inputs.subdiv.value = -3;
  neg.update();
  CHECK(neg.outputs.geometry.mesh.vertices == 60 * 4);
}

TEST_CASE("Cylinder and Torus clamp their divisions", "[threedim][primitive]")
{
  Threedim::Cylinder a, b;
  a.inputs.slices.value = 64;
  a.inputs.stacks.value = 64;
  a.update();
  b.inputs.slices.value = 100;
  b.inputs.stacks.value = 100;
  b.update();
  CHECK(b.outputs.geometry.mesh.vertices == a.outputs.geometry.mesh.vertices);

  Threedim::Torus t, u;
  t.inputs.hdiv.value = 50;
  t.inputs.vdiv.value = 50;
  t.update();
  u.inputs.hdiv.value = 80;
  u.inputs.vdiv.value = 80;
  u.update();
  CHECK(u.outputs.geometry.mesh.vertices == t.outputs.geometry.mesh.vertices);
}

// Threedim::EnvironmentLoader's fog controls: every mode and the density
// reach scene_environment::fog, which ScenePreprocessor packs into the env
// UBO (fog_range.z = mode, fog_color_density.w = density) that
// score_pbr_env.glsl reads, and merge_scenes carries them with the fog group.

#include <Threedim/EnvironmentLoader.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <span>

using Catch::Approx;
using fog_type = decltype(ossia::scene_environment::fog)::type;

TEST_CASE("EnvironmentLoader publishes the fog mode and density",
          "[threedim][environment]")
{
  Threedim::EnvironmentLoader env;
  env.inputs.fog_enabled.value = true;
  env.inputs.fog_density.value = 0.25f;

  env.inputs.fog_mode.value = Threedim::EnvironmentLoader::FogLinear;
  env.rebuild();
  env();
  REQUIRE(env.m_state);
  CHECK(env.m_state->environment.fog.mode == fog_type::linear);
  CHECK(env.m_state->environment.fog.density == Approx(0.25f));

  env.inputs.fog_mode.value = Threedim::EnvironmentLoader::FogExponential;
  env.rebuild();
  env();
  CHECK(env.m_state->environment.fog.mode == fog_type::exponential);

  env.inputs.fog_mode.value = Threedim::EnvironmentLoader::FogExponentialSquared;
  env.inputs.fog_density.value = 0.5f;
  env.rebuild();
  env();
  const auto& fog = env.m_state->environment.fog;
  CHECK(fog.mode == fog_type::exponential_squared);
  CHECK(fog.density == Approx(0.5f));
  CHECK(fog.enabled);
  // The shaders read the mode as a float compared against 0.5 and 1.5.
  CHECK(float(fog.mode) == 2.f);

  // The fog group survives the merge onto a scene from another producer.
  auto plain = std::make_shared<ossia::scene_state>();
  plain->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  plain->version = 1;
  std::array<ossia::scene_spec, 2> specs;
  specs[0].state = plain;
  specs[1] = env.outputs.scene_out.scene;
  const auto merged
      = ossia::merge_scenes(std::span<const ossia::scene_spec>{specs});
  REQUIRE(merged.state);
  CHECK(merged.state->environment.fog.mode == fog_type::exponential_squared);
  CHECK(merged.state->environment.fog.density == Approx(0.5f));
}

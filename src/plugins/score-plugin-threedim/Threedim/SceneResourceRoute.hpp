#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>

namespace Threedim
{

// Level 1 of the "resource → scene field" routing design. Takes a GPU
// texture handle from any upstream producer (CSF shader output, video,
// ISF post-pass, asset loader, …) and stamps it onto a named field of
// `scene_spec`. The emitted scene_spec is a partial contribution with
// only that one field populated — `merge_scenes` overlays it onto the
// rest of the scene_state the ScenePreprocessor receives from other
// producers, so this node composes freely with EnvironmentLoader /
// CubemapLoader / further SceneResourceRoute instances.
//
// Core use case is IBL wiring: an IrradianceConvolve / PrefilterGGX /
// BrdfLut shader's output plugs in here and lands on
// `scene_environment.{irradiance_map, prefiltered_map, brdf_lut}` with
// zero bespoke glue code per target. Shadow-map generation passes will
// target `scene_state.shadow_cascades.shadow_map_array` the same way.
//
// Pattern mirrors CubemapComposer / InjectTexture: CPU-side producer,
// port-driven rebuild + handle-change detection in operator()().
enum class SceneResourceTarget : int
{
  Skybox,           // scene_environment.skybox_texture
  IrradianceMap,    // scene_environment.irradiance_map
  PrefilteredMap,   // scene_environment.prefiltered_map
  BRDFLut,          // scene_environment.brdf_lut
  ShadowMapArray,   // scene_state.shadow_cascades.shadow_map_array
};

class SceneResourceRoute
{
public:
  halp_meta(name, "Scene Resource Route")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "scene_resource_route")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/scene-resource-route.html")
  halp_meta(uuid, "c2f7a341-8e69-4b0d-b3f8-2d7e4c5a9f1b")

  struct ins
  {
    // Accepts any GPU texture kind — 2D, cubemap, array. Downstream
    // consumer shaders (classic_pbr_ibl, classic_pbr_shadowed) declare
    // their own sampler shape (samplerCube / sampler2DArray / sampler2D)
    // and it's the authoring's responsibility to match the two.
    halp::gpu_texture_input<"Texture"> texture;

    // Port-driven rebuild: target changes fire rebuild(); upstream
    // handle flips are caught by operator()() since the halp GPU-texture
    // input doesn't emit a port-update event when only the native
    // handle swaps.
    struct : halp::enum_t<SceneResourceTarget, "Target Field">
    {
      void update(SceneResourceRoute& n) { n.rebuild(); }
    } target;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  void rebuild();
  void operator()();

  // Cached output scene_state — stable identity across frames (so
  // downstream scene-identity caches stay hot) and mutated in place on
  // target / handle changes.
  std::shared_ptr<ossia::scene_state> m_state;
  int64_t m_version{0};
  void* m_cached_handle{};
  SceneResourceTarget m_cached_target{SceneResourceTarget::Skybox};
  uint8_t m_pending_dirty{0xFF};
};

}

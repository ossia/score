#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <cstdint>
#include <memory>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// Scene-producing node that defines the environment of a scene:
// ambient light, exposure, gamma, fog.
//
// Pairs with the project-wide scene_spec merge rule: environment is
// merged field-by-field using the params_set bitmask. This node sets
// only ambient / exposure-gamma / fog bits — skybox texture and IBL
// handles are owned by CubemapLoader / CubemapComposer (and a future
// EnvironmentPrecompute for real IBL) and overlay cleanly via
// merge_scenes.
//
// Downstream pipeline:
//   - `ossia::merge_scenes` overlays this environment onto the merged
//     scene_state — field groups without matching bits pass through
//     from whichever producer set them.
//   - ScenePreprocessor packs scene_environment fields into an Environment
//     Params UBO (auto-bound as aux buffer on Geometry Out).
//   - classic_pbr_ibl shaders read the UBO for ambient / exposure / fog.
class EnvironmentLoader
{
public:
  halp_meta(name, "Environment")
  halp_meta(c_name, "environment_loader")
  halp_meta(category, "Visuals/3D")
  halp_meta(authors, "ossia team")
  halp_meta(uuid, "d3f5a8c1-8b47-4e91-9c2d-6f1a9b5e3c82")

  struct ins
  {
    // Port-driven rebuild: each control's update() callback fires only
    // on real change, triggering EnvironmentLoader::rebuild().
    struct : halp::xyz_spinboxes_f32<"Ambient Color", halp::range{0., 1., 0.03}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } ambient_color;
    struct : halp::hslider_f32<"Ambient Intensity", halp::range{0., 8., 1.}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } ambient_intensity;

    // Photographic exposure value at ISO 100. Describes the scene's
    // expected brightness in photometric terms; downstream shaders
    // compensate so brighter scenes (higher EV100) display darker
    // without manual rebalancing. Reference values:
    //   EV100 ≈ -3   moonlit night
    //   EV100 ≈  3   indoor lighting
    //   EV100 ≈ 12   midday outdoor
    //   EV100 ≈ 16   direct sunlight
    // EV100 = 0 leaves the linear multiplier at 1× (combined with the
    // default exposure_stops below it), preserving backward
    // compatibility with scenes authored before EV100 existed.
    struct : halp::hslider_f32<"Exposure EV100", halp::range{-6., 18., 0.}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } ev100;

    // Fine-tune compensation atop EV100, in stops (±EV). Same role as
    // a photographer's "exposure compensation" dial: ev100 sets the
    // photographic anchor, exposure_stops biases above/below.
    struct : halp::hslider_f32<"Exposure (stops)", halp::range{-8., 8., 0.}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } exposure_stops;
    struct : halp::hslider_f32<"Gamma", halp::range{1., 3., 2.2}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } gamma;

    struct : halp::toggle<"Fog">
    { void update(EnvironmentLoader& n) { n.rebuild(); } } fog_enabled;
    struct : halp::xyz_spinboxes_f32<"Fog Color", halp::range{0., 1., 0.8}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } fog_color;
    struct : halp::hslider_f32<"Fog Start", halp::range{0., 1000., 10.}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } fog_start;
    struct : halp::hslider_f32<"Fog End", halp::range{0., 10000., 100.}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } fog_end;

    // Downstream render-target dimensions (width, height). Stamped on
    // scene_environment::render_target_size + params_render_target_size
    // bit when both values > 0. Overrides the preprocessor's default
    // derivation from the RenderList swap chain.
    struct : halp::xy_spinboxes_i32<"Render target size", halp::range{0, 16384, 0}>
    { void update(EnvironmentLoader& n) { n.rebuild(); } } render_target_size;
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

  // Rebuild m_state from current inputs. Invoked by each port's
  // update() callback on real control changes. operator()() just
  // republishes m_state, so the emitted shared_ptr + version stay
  // stable when nothing changed — keeps every downstream cache hot.
  void rebuild();
  void operator()();

  // Render-thread GPU hooks, invoked by CpuFilterNode. init allocates a
  // slot in the Env arena once; update rebuilds the EnvParamsUBO bytes
  // and uploads them into the slot (ScenePreprocessor will later pick
  // these up directly instead of repacking the CPU struct — producer
  // half only for now); release returns the slot.
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  // Invariant identity for the shared scene_environment struct we emit —
  // holding one stable scene_state across frames lets downstream
  // `scene.state.get()` comparisons short-circuit the no-op case. We
  // mutate the state's environment in place on parameter changes.
  std::shared_ptr<ossia::scene_state> m_state;
  int64_t m_version{0};
  uint8_t m_pending_dirty{ossia::scene_port::dirty_environment};

  // Slot in RenderList::registry().buffer(Env). Allocated in init(),
  // written in update(), freed in release().
  score::gfx::GpuResourceRegistry::Slot env_slot;

  // Ossia-facing snapshot of env_slot, stamped on scene_state::
  // environment.raw_slot in operator()() so the preprocessor can
  // resolve our slot via isLive(). Written once in init() on the
  // render thread, read every tick in operator()() on the execution
  // thread (trivially-copyable POD, initialised to zero so pre-init
  // reads look like an invalid ref).
  ossia::gpu_slot_ref m_env_ref{};
};

}

#include "EnvironmentLoader.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <cmath>

namespace Threedim
{

void EnvironmentLoader::rebuild()
{
  if(!m_state)
  {
    m_state = std::make_shared<ossia::scene_state>();
    m_state->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  }

  auto& env = m_state->environment;
  // Reset: this node only sets the ambient / exposure / gamma / fog
  // groups. It does NOT touch skybox_texture / IBL handles — those
  // come from cube-texture producers (CubemapLoader, …) that emit
  // their own scene_spec with only the relevant fields populated.
  // merge_scenes overlays field-by-field using the params_set mask.
  env = {};

  env.ambient_color[0] = inputs.ambient_color.value.x;
  env.ambient_color[1] = inputs.ambient_color.value.y;
  env.ambient_color[2] = inputs.ambient_color.value.z;
  env.ambient_intensity = inputs.ambient_intensity.value;
  // Photographic exposure: EV100 is the scene anchor, exposure_stops is
  // the user-facing fine-tune (analogous to a camera's ±EV dial). The
  // standard formula is `mul = stops_gain / (K * 2^EV100)`; we use K=1
  // so EV100 = 0, stops = 0 leaves `env.exposure = 1` (preserving
  // backward compat with scenes from before EV100 existed). Switch to
  // the photometric K=1.2 (Frostbite/UE/Filament) once tone-mapping
  // post-processes are the norm — at that point a non-unit default
  // multiplier stops being surprising.
  constexpr float K = 1.0f;
  env.exposure = std::exp2(inputs.exposure_stops.value)
               / (K * std::exp2(inputs.ev100.value));
  env.gamma = inputs.gamma.value;
  env.fog.enabled = inputs.fog_enabled.value;
  env.fog.color[0] = inputs.fog_color.value.x;
  env.fog.color[1] = inputs.fog_color.value.y;
  env.fog.color[2] = inputs.fog_color.value.z;
  env.fog.start = inputs.fog_start.value;
  env.fog.end = inputs.fog_end.value;

  env.params_set = ossia::scene_environment::params_ambient
                   | ossia::scene_environment::params_exposure_gamma
                   | ossia::scene_environment::params_fog;

  // Render target size: only publish the overlay when both dimensions
  // are positive. 0,0 (the default) means "let downstream fall back to
  // the RenderList swap-chain size" — don't stamp the bit so other
  // branches with legitimate sizes can still win the merge.
  if(inputs.render_target_size.value.x > 0
     && inputs.render_target_size.value.y > 0)
  {
    env.render_target_size[0] = (uint32_t)inputs.render_target_size.value.x;
    env.render_target_size[1] = (uint32_t)inputs.render_target_size.value.y;
    env.params_set |= ossia::scene_environment::params_render_target_size;
  }

  // Propagate the Env arena slot ref so the preprocessor can resolve
  // our slot via ossia::gpu_slot_ref. m_env_ref is populated once in
  // init() on the render thread — here on the execution thread we
  // just copy the POD value. It stays zero (invalid) until init() runs,
  // which is fine: preprocessor's isLive() will reject a zero ref.
  env.raw_slot = m_env_ref;

  m_version++;
  m_state->version = m_version;
  m_pending_dirty = ossia::scene_port::dirty_environment;
}

void EnvironmentLoader::operator()()
{
  if(!m_state)
    rebuild();
  outputs.scene_out.scene.state = m_state;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

void EnvironmentLoader::init(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  // Claim one slot in the Env arena for this node's lifetime. Kept in
  // env_slot; released in release() below. The slot's offset + buffer
  // are stable — consumer shaders bind r.registry().buffer(Env) with
  // registry.slotOffset(env_slot) as the range base.
  if(!env_slot.valid())
  {
    env_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::Env,
        sizeof(score::gfx::EnvParamsUBO));
    m_env_ref = r.registry().toOssiaRef(env_slot);
  }
  // Seed the slot with default-constructed bytes so downstream consumers
  // that sample the slot before operator()() has ever run see a sane
  // neutral environment rather than undefined memory.
  if(env_slot.valid())
  {
    score::gfx::EnvParamsUBO seed{};
    r.registry().updateSlot(res, env_slot, &seed, sizeof(seed));
  }
}

void EnvironmentLoader::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
    score::gfx::Edge*)
{
  // Render-thread path: pack the current CPU-side scene_environment into
  // the EnvParamsUBO layout and upload to our slot. CpuFilterNode runs
  // processControlIn before calling us, so `inputs.*.value` already
  // reflects the latest control state — and operator()() has already
  // run this frame, so m_state->environment holds the freshest data.
  if(!env_slot.valid() || !m_state)
    return;

  const auto& env = m_state->environment;
  score::gfx::EnvParamsUBO gpu{};
  gpu.ambient[0] = env.ambient_color[0];
  gpu.ambient[1] = env.ambient_color[1];
  gpu.ambient[2] = env.ambient_color[2];
  gpu.ambient[3] = env.ambient_intensity;
  gpu.fog_color_density[0] = env.fog.color[0];
  gpu.fog_color_density[1] = env.fog.color[1];
  gpu.fog_color_density[2] = env.fog.color[2];
  gpu.fog_color_density[3] = env.fog.density;
  gpu.fog_range[0] = env.fog.start;
  gpu.fog_range[1] = env.fog.end;
  gpu.fog_range[2] = float(env.fog.mode);
  gpu.fog_range[3] = env.fog.enabled ? 1.f : 0.f;
  gpu.exposure_gamma[0] = env.exposure;
  gpu.exposure_gamma[1] = env.gamma;
  gpu.exposure_gamma[2] = 0.f;
  gpu.exposure_gamma[3] = 0.f;
  r.registry().updateSlot(res, env_slot, &gpu, sizeof(gpu));
}

void EnvironmentLoader::release(score::gfx::RenderList& r)
{
  if(env_slot.valid())
    r.registry().free(env_slot);
  m_env_ref = {};
  // Producer-state-drift Option A — see Light::release.
  m_state.reset();
}

}

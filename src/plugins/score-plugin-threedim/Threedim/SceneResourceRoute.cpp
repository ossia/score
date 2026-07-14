#include "SceneResourceRoute.hpp"

namespace Threedim
{

void SceneResourceRoute::rebuild()
{
  if(!m_state)
    m_state = std::make_shared<ossia::scene_state>();

  // Reset the environment / shadow bits we own. Partial-producer
  // contract: this node contributes exactly one field; everything else
  // stays at defaults and gets filtered out by merge_scenes' per-field
  // overlay (it only picks up texture handles with non-null
  // native_handle, and params_set bits we don't light — we don't).
  m_state->environment = {};
  m_state->shadow_cascades = {};

  void* handle = inputs.texture.texture.handle;
  if(handle)
  {
    switch(inputs.target.value)
    {
      case SceneResourceTarget::Skybox:
        m_state->environment.skybox_texture.native_handle = handle;
        break;
      case SceneResourceTarget::IrradianceMap:
        m_state->environment.irradiance_map.native_handle = handle;
        break;
      case SceneResourceTarget::PrefilteredMap:
        m_state->environment.prefiltered_map.native_handle = handle;
        break;
      case SceneResourceTarget::BRDFLut:
        m_state->environment.brdf_lut.native_handle = handle;
        break;
      case SceneResourceTarget::ShadowMapArray:
        m_state->shadow_cascades.shadow_map_array.native_handle = handle;
        break;
    }
  }

  m_state->version = ++m_version;
  m_state->dirty_index = m_version;

  m_cached_handle = handle;
  m_cached_target = inputs.target.value;
  m_pending_dirty = 0xFF;
}

void SceneResourceRoute::operator()()
{
  // The halp GPU-texture input doesn't fire a port-update event on
  // native-handle swap (only on port re-wiring), so we poll here and
  // rebuild when either the handle or the target changed. Stable
  // scene_state identity means the no-change case is a cheap
  // shared_ptr forward without re-allocating.
  void* handle = inputs.texture.texture.handle;
  const bool changed = !m_state || handle != m_cached_handle
                       || inputs.target.value != m_cached_target;
  if(changed)
    rebuild();

  outputs.scene_out.scene.state = m_state;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim

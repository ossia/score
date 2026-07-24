#include "Transform3D.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <QQuaternion>

namespace Threedim
{

void Transform3D::operator()()
{
  const auto& in = inputs.scene_in.scene;
  const auto* in_state = in.state.get();

  if(!in_state || in_state->empty())
  {
    outputs.scene_out.scene = {};
    outputs.scene_out.dirty = 0;
    m_state.reset();
    m_cached_in_state = nullptr;
    m_cached_in_version = -1;
    m_cachedTRS.valid = false;
    return;
  }

  // Cache check: republish the prior wrapped state when neither upstream
  // (state pointer / version) nor TRS controls changed. Stops downstream
  // identity-keyed caches from rebuilding every frame on a stable input —
  // see diagnostic 027.
  const int64_t in_version = in_state->version;
  const bool upstream_changed
      = (m_cached_in_state != in_state) || (m_cached_in_version != in_version);
  const bool trs_changed = transformChanged(inputs, m_cachedTRS);

  if(m_state && !upstream_changed && !trs_changed)
  {
    outputs.scene_out.scene.state = m_state;
    outputs.scene_out.dirty = 0;
    return;
  }

  // Rebuild via the canonical helper: it now propagates skeletons and
  // collections too (diagnostic 026), updates m_cachedTRS in place, and
  // bumps m_version_counter so downstream version-keyed caches see a
  // monotonic bump exactly when something actually changed.
  m_state = wrapSceneWithTransform(
      in.state, inputs, m_cachedTRS, m_version_counter, m_xform_ref);
  m_cached_in_state = in_state;
  m_cached_in_version = in_version;

  outputs.scene_out.scene.state = m_state;
  outputs.scene_out.dirty = 0xFF;
}

// Order invariant: called by GfxRenderer::initState BEFORE the first
// operator()() and BEFORE processControlIn fires any rebuild() callback.
// m_xform_ref populated here is therefore safe to read in rebuild()
// without a guard. Adding prepare() to this node breaks the invariant —
// see CpuFilterNode.hpp for details.
void Transform3D::init(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  if(!xform_slot.valid())
  {
    xform_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawTransform,
        sizeof(score::gfx::RawLocalTransform));
    m_xform_ref = r.registry().toOssiaRef(xform_slot);
  }
  if(xform_slot.valid())
  {
    score::gfx::RawLocalTransform seed{};
    r.registry().updateSlot(res, xform_slot, &seed, sizeof(seed));
  }
}

void Transform3D::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(!xform_slot.valid())
    return;

  score::gfx::RawLocalTransform raw{};
  raw.translation[0] = inputs.position.value.x;
  raw.translation[1] = inputs.position.value.y;
  raw.translation[2] = inputs.position.value.z;
  auto q = QQuaternion::fromEulerAngles(
      inputs.rotation.value.x, inputs.rotation.value.y,
      inputs.rotation.value.z);
  raw.rotation[0] = q.x();
  raw.rotation[1] = q.y();
  raw.rotation[2] = q.z();
  raw.rotation[3] = q.scalar();
  raw.scale[0] = inputs.scale.value.x;
  raw.scale[1] = inputs.scale.value.y;
  raw.scale[2] = inputs.scale.value.z;
  r.registry().updateSlot(res, xform_slot, &raw, sizeof(raw));
}

void Transform3D::release(score::gfx::RenderList& r)
{
  if(xform_slot.valid())
    r.registry().free(xform_slot);
  m_xform_ref = {};
  // Clear cached scene_state so the next operator()() rebuilds against
  // the post-release registry. Producer-state-drift Option A — see
  // matching comment in Light::release.
  m_state.reset();
  m_cached_in_state = nullptr;
}

}

#include "Camera.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

namespace Threedim
{

// Order invariant: called by GfxRenderer::initState BEFORE the first
// operator()() and BEFORE processControlIn fires any rebuild() callback.
// m_camera_ref / m_xform_ref populated here are therefore safe to read
// in rebuild() without a guard. Adding prepare() to this node breaks the
// invariant — see CpuFilterNode.hpp for details.
void Camera::init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  if(!raw_camera_slot.valid())
  {
    raw_camera_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawCamera,
        sizeof(score::gfx::RawCameraData));
    m_camera_ref = r.registry().toOssiaRef(raw_camera_slot);
  }
  if(raw_camera_slot.valid())
  {
    score::gfx::RawCameraData seed{};
    r.registry().updateSlot(res, raw_camera_slot, &seed, sizeof(seed));
  }
  if(!raw_transform_slot.valid())
  {
    raw_transform_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawTransform,
        sizeof(score::gfx::RawLocalTransform));
    m_xform_ref = r.registry().toOssiaRef(raw_transform_slot);
  }
  if(raw_transform_slot.valid())
  {
    score::gfx::RawLocalTransform seed{};
    r.registry().updateSlot(res, raw_transform_slot, &seed, sizeof(seed));
  }
}

void Camera::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(raw_transform_slot.valid())
  {
    // Local TRS of the scene_transform this camera emits. Translation
    // mirrors the eye; rotation matches the quaternion built in
    // operator()() (local -Z → (target - eye)); scale is identity.
    score::gfx::RawLocalTransform xform{};
    xform.translation[0] = inputs.eye.value.x;
    xform.translation[1] = inputs.eye.value.y;
    xform.translation[2] = inputs.eye.value.z;
    QVector3D forward(
        inputs.target.value.x - inputs.eye.value.x,
        inputs.target.value.y - inputs.eye.value.y,
        inputs.target.value.z - inputs.eye.value.z);
    if(forward.lengthSquared() > 1e-8f)
    {
      forward.normalize();
      QQuaternion q = QQuaternion::fromDirection(
          -forward, QVector3D(0.f, 1.f, 0.f));
      xform.rotation[0] = q.x();
      xform.rotation[1] = q.y();
      xform.rotation[2] = q.z();
      xform.rotation[3] = q.scalar();
    }
    xform.scale[0] = 1.f;
    xform.scale[1] = 1.f;
    xform.scale[2] = 1.f;
    r.registry().updateSlot(res, raw_transform_slot, &xform, sizeof(xform));
  }

  if(!raw_camera_slot.valid())
    return;

  score::gfx::RawCameraData raw{};
  raw.eye[0] = inputs.eye.value.x;
  raw.eye[1] = inputs.eye.value.y;
  raw.eye[2] = inputs.eye.value.z;
  raw.target[0] = inputs.target.value.x;
  raw.target[1] = inputs.target.value.y;
  raw.target[2] = inputs.target.value.z;
  raw.up[0] = 0.f;
  raw.up[1] = 1.f;
  raw.up[2] = 0.f;
  raw.yfov = inputs.fov.value * float(M_PI) / 180.f;
  raw.znear = inputs.near_plane.value;
  raw.zfar = inputs.far_plane.value;
  raw.projection = 0u;  // perspective
  r.registry().updateSlot(res, raw_camera_slot, &raw, sizeof(raw));
}

void Camera::release(score::gfx::RenderList& r)
{
  if(raw_camera_slot.valid())
    r.registry().free(raw_camera_slot);
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_camera_ref = {};
  m_xform_ref = {};
  // Clear the cached scene_state so the next operator()() re-runs
  // rebuild() against the freshly-allocated arena slots. Without this,
  // an in-place release+init cycle (viewport resize / relinkGraph)
  // republishes the old m_state whose scene_transform.raw_slot still
  // embeds the OLD (now-freed) RawTransform index. The preprocessor's
  // flatten gates worldTransforms emission on raw_slot.size != 0 (not
  // isLive()), so it accepts the stale index and writes the camera's
  // world matrix into a slot that init() may have re-allocated to
  // another producer → aliased world-transforms that drift each cycle.
  // Matches Light::release / Transform3D::release / CameraArray::release.
  m_state.reset();
}

}

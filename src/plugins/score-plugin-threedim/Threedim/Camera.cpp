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
    score::gfx::RawLocalTransform xform{};
    const QVector3D t = position();
    xform.translation[0] = t.x();
    xform.translation[1] = t.y();
    xform.translation[2] = t.z();
    const QQuaternion q = orientation();
    xform.rotation[0] = q.x();
    xform.rotation[1] = q.y();
    xform.rotation[2] = q.z();
    xform.rotation[3] = q.scalar();
    xform.scale[0] = 1.f;
    xform.scale[1] = 1.f;
    xform.scale[2] = 1.f;
    r.registry().updateSlot(res, raw_transform_slot, &xform, sizeof(xform));
  }

  if(!raw_camera_slot.valid())
    return;

  score::gfx::RawCameraData raw{};
  const QVector3D eye = position();
  const QQuaternion q = orientation();
  const QVector3D target = inputs.pose.value == ins::LookAt && !screenMode()
                               ? QVector3D(
                                   inputs.target.value.x, inputs.target.value.y,
                                   inputs.target.value.z)
                               : eye + q.rotatedVector(QVector3D(0.f, 0.f, -1.f));
  raw.eye[0] = eye.x();
  raw.eye[1] = eye.y();
  raw.eye[2] = eye.z();
  raw.target[0] = target.x();
  raw.target[1] = target.y();
  raw.target[2] = target.z();
  const QVector3D up = q.rotatedVector(QVector3D(0.f, 1.f, 0.f));
  raw.up[0] = up.x();
  raw.up[1] = up.y();
  raw.up[2] = up.z();
  ossia::camera_component cam;
  fillProjection(cam);
  raw.yfov = cam.yfov;
  raw.znear = cam.znear;
  raw.zfar = cam.zfar;
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
  // raw_slot indices from the previous registry must not be republished
  // after a release+init cycle.
  m_state.reset();
}

}

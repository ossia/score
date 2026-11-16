#include "CameraArray.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <array>

namespace Threedim
{

// Order invariant: called by GfxRenderer::initState BEFORE the first
// operator()() and BEFORE processControlIn fires any rebuild() callback.
// m_array_ref populated here is therefore safe to read in rebuild()
// without a guard. Adding prepare() to this node breaks the invariant —
// see CpuFilterNode.hpp for details.
void CameraArray::init(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  // One slot per face: the RawCamera / RawTransform arenas are
  // fixed-stride (one 64-byte entry per slot), so a contiguous 6-wide
  // block cannot be allocated — allocate() rejects any size above the
  // stride and the whole array would silently produce no GPU data.
  for(int i = 0; i < 6; ++i)
  {
    if(!raw_camera_slot[i].valid())
    {
      raw_camera_slot[i] = r.registry().allocate(
          score::gfx::GpuResourceRegistry::Arena::RawCamera,
          sizeof(score::gfx::RawCameraData));
      m_array_ref[i] = r.registry().toOssiaRef(raw_camera_slot[i]);
    }
    if(raw_camera_slot[i].valid())
    {
      score::gfx::RawCameraData seed{};
      r.registry().updateSlot(res, raw_camera_slot[i], &seed, sizeof(seed));
    }
    if(!raw_transform_slot[i].valid())
    {
      raw_transform_slot[i] = r.registry().allocate(
          score::gfx::GpuResourceRegistry::Arena::RawTransform,
          sizeof(score::gfx::RawLocalTransform));
      m_xform_array_ref[i] = r.registry().toOssiaRef(raw_transform_slot[i]);
    }
    if(raw_transform_slot[i].valid())
    {
      score::gfx::RawLocalTransform seed{};
      r.registry().updateSlot(res, raw_transform_slot[i], &seed, sizeof(seed));
    }
  }
}

void CameraArray::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(!raw_camera_slot[0].valid())
    return;

  const float eye[3]{
      inputs.origin.value.x, inputs.origin.value.y, inputs.origin.value.z};
  const float znear = inputs.near_plane.value;
  const float zfar = inputs.far_plane.value;

  const QQuaternion rig = rigRotation();
  score::gfx::RawCameraData raw[6]{};
  for(int i = 0; i < 6; ++i)
  {
    raw[i].eye[0] = eye[0];
    raw[i].eye[1] = eye[1];
    raw[i].eye[2] = eye[2];
    const QQuaternion q = faceRotation(i, rig);
    const QVector3D fwd = q.rotatedVector({0.f, 0.f, -1.f});
    const QVector3D up = q.rotatedVector({0.f, 1.f, 0.f});
    raw[i].target[0] = eye[0] + fwd.x();
    raw[i].target[1] = eye[1] + fwd.y();
    raw[i].target[2] = eye[2] + fwd.z();
    raw[i].up[0] = up.x();
    raw[i].up[1] = up.y();
    raw[i].up[2] = up.z();
    raw[i].yfov = float(M_PI) / 2.f;  // 90° per face
    raw[i].znear = znear;
    raw[i].zfar = zfar;
    raw[i].projection = 0u;  // perspective
  }
  for(int i = 0; i < 6; ++i)
    if(raw_camera_slot[i].valid())
      r.registry().updateSlot(res, raw_camera_slot[i], &raw[i], sizeof(raw[i]));

  if(raw_transform_slot[0].valid())
  {
    score::gfx::RawLocalTransform xforms[6]{};
    for(int i = 0; i < 6; ++i)
    {
      xforms[i].translation[0] = eye[0];
      xforms[i].translation[1] = eye[1];
      xforms[i].translation[2] = eye[2];
      const QQuaternion q = faceRotation(i, rig);
      xforms[i].rotation[0] = q.x();
      xforms[i].rotation[1] = q.y();
      xforms[i].rotation[2] = q.z();
      xforms[i].rotation[3] = q.scalar();
      xforms[i].scale[0] = 1.f;
      xforms[i].scale[1] = 1.f;
      xforms[i].scale[2] = 1.f;
    }
    for(int i = 0; i < 6; ++i)
      if(raw_transform_slot[i].valid())
        r.registry().updateSlot(
            res, raw_transform_slot[i], &xforms[i], sizeof(xforms[i]));
  }
}

void CameraArray::release(score::gfx::RenderList& r)
{
  for(int i = 0; i < 6; ++i)
  {
    if(raw_camera_slot[i].valid())
      r.registry().free(raw_camera_slot[i]);
    raw_camera_slot[i] = {};
    if(raw_transform_slot[i].valid())
      r.registry().free(raw_transform_slot[i]);
    raw_transform_slot[i] = {};
    m_array_ref[i] = {};
    m_xform_array_ref[i] = {};
  }
  // raw_slot indices from the previous registry must not be republished
  // after a release+init cycle.
  m_state.reset();
}

}

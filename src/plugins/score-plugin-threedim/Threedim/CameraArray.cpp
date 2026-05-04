#include "CameraArray.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <array>

namespace Threedim
{

namespace
{
// Same face layout as operator()() — keep the two definitions in sync;
// FlattenVisitor pulls scene_transform from the scene_spec emission,
// ScenePreprocessor will (later) consume the raw slots here.
struct Face
{
  float forward[3];
  float up[3];
};
constexpr std::array<Face, 6> kFaces{{
    {{ 1.f,  0.f,  0.f}, {0.f, -1.f,  0.f}},  // +X
    {{-1.f,  0.f,  0.f}, {0.f, -1.f,  0.f}},  // -X
    {{ 0.f,  1.f,  0.f}, {0.f,  0.f,  1.f}},  // +Y
    {{ 0.f, -1.f,  0.f}, {0.f,  0.f, -1.f}},  // -Y
    {{ 0.f,  0.f,  1.f}, {0.f, -1.f,  0.f}},  // +Z
    {{ 0.f,  0.f, -1.f}, {0.f, -1.f,  0.f}},  // -Z
}};
}

void CameraArray::init(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  if(!raw_camera_slot.valid())
  {
    raw_camera_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawCamera,
        6 * sizeof(score::gfx::RawCameraData));
    m_array_ref = r.registry().toOssiaRef(raw_camera_slot);
  }
  if(raw_camera_slot.valid())
  {
    score::gfx::RawCameraData seed[6]{};
    r.registry().updateSlot(res, raw_camera_slot, &seed, sizeof(seed));
  }
  if(!raw_transform_slot.valid())
  {
    raw_transform_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawTransform,
        6 * sizeof(score::gfx::RawLocalTransform));
    m_xform_array_ref = r.registry().toOssiaRef(raw_transform_slot);
  }
  if(raw_transform_slot.valid())
  {
    score::gfx::RawLocalTransform seed[6]{};
    r.registry().updateSlot(res, raw_transform_slot, &seed, sizeof(seed));
  }
}

void CameraArray::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(!raw_camera_slot.valid())
    return;

  const float eye[3]{
      inputs.origin.value.x, inputs.origin.value.y, inputs.origin.value.z};
  const float znear = inputs.near_plane.value;
  const float zfar = inputs.far_plane.value;

  score::gfx::RawCameraData raw[6]{};
  for(int i = 0; i < 6; ++i)
  {
    raw[i].eye[0] = eye[0];
    raw[i].eye[1] = eye[1];
    raw[i].eye[2] = eye[2];
    raw[i].target[0] = eye[0] + kFaces[i].forward[0];
    raw[i].target[1] = eye[1] + kFaces[i].forward[1];
    raw[i].target[2] = eye[2] + kFaces[i].forward[2];
    raw[i].up[0] = kFaces[i].up[0];
    raw[i].up[1] = kFaces[i].up[1];
    raw[i].up[2] = kFaces[i].up[2];
    raw[i].yfov = float(M_PI) / 2.f;  // 90° per face
    raw[i].znear = znear;
    raw[i].zfar = zfar;
    raw[i].projection = 0u;  // perspective
  }
  r.registry().updateSlot(res, raw_camera_slot, &raw, sizeof(raw));

  if(raw_transform_slot.valid())
  {
    // Per-face scene_transform local TRS: translation = origin;
    // rotation from -forward via QQuaternion::fromDirection (same as
    // the scene_spec emission path). scale = identity.
    score::gfx::RawLocalTransform xforms[6]{};
    for(int i = 0; i < 6; ++i)
    {
      xforms[i].translation[0] = eye[0];
      xforms[i].translation[1] = eye[1];
      xforms[i].translation[2] = eye[2];
      QVector3D fwd(
          kFaces[i].forward[0], kFaces[i].forward[1], kFaces[i].forward[2]);
      QVector3D up(kFaces[i].up[0], kFaces[i].up[1], kFaces[i].up[2]);
      QQuaternion q = QQuaternion::fromDirection(-fwd, up);
      xforms[i].rotation[0] = q.x();
      xforms[i].rotation[1] = q.y();
      xforms[i].rotation[2] = q.z();
      xforms[i].rotation[3] = q.scalar();
      xforms[i].scale[0] = 1.f;
      xforms[i].scale[1] = 1.f;
      xforms[i].scale[2] = 1.f;
    }
    r.registry().updateSlot(
        res, raw_transform_slot, &xforms, sizeof(xforms));
  }
}

void CameraArray::release(score::gfx::RenderList& r)
{
  if(raw_camera_slot.valid())
    r.registry().free(raw_camera_slot);
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_array_ref = {};
  m_xform_array_ref = {};
}

}

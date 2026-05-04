#include "Transform3D.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <QQuaternion>

namespace Threedim
{

void Transform3D::operator()()
{
  if(!inputs.scene_in.scene.state
     || inputs.scene_in.scene.state->empty())
  {
    outputs.scene_out.scene = {};
    outputs.scene_out.dirty = 0;
    return;
  }

  const auto& in = inputs.scene_in.scene;

  // Build the TRS payload. QQuaternion::fromEulerAngles takes (pitch, yaw,
  // roll) in degrees.
  ossia::scene_transform xform;
  xform.translation[0] = inputs.position.value.x;
  xform.translation[1] = inputs.position.value.y;
  xform.translation[2] = inputs.position.value.z;
  auto q = QQuaternion::fromEulerAngles(
      inputs.rotation.value.x, inputs.rotation.value.y,
      inputs.rotation.value.z);
  xform.rotation[0] = q.x();
  xform.rotation[1] = q.y();
  xform.rotation[2] = q.z();
  xform.rotation[3] = q.scalar();
  xform.scale[0] = inputs.scale.value.x;
  xform.scale[1] = inputs.scale.value.y;
  xform.scale[2] = inputs.scale.value.z;
  // Propagate the RawTransform slot ref so the preprocessor can write
  // the composed world matrix at the matching WorldTransform offset.
  xform.raw_slot = m_xform_ref;

  // Wrap roots under a single parent whose first child is the transform
  // payload. Transforms apply to subsequent siblings in visitor order,
  // so the single-transform + roots layout carries the TRS to everything.
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(xform);
  if(in.state->roots)
  {
    for(auto& root : *in.state->roots)
      children->push_back(root);
  }

  auto parent = std::make_shared<ossia::scene_node>();
  parent->children = std::move(children);

  auto new_roots
      = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  new_roots->push_back(std::move(parent));

  auto new_state = std::make_shared<ossia::scene_state>();
  new_state->roots = std::move(new_roots);
  // Identity-preserving passthrough of shared state.
  if(in.state->materials)
    new_state->materials = in.state->materials;
  if(in.state->animations)
    new_state->animations = in.state->animations;
  if(in.state->cameras)
    new_state->cameras = in.state->cameras;
  new_state->environment = in.state->environment;
  new_state->active_camera_id = in.state->active_camera_id;
  new_state->version = in.state->version;

  outputs.scene_out.scene.state = std::move(new_state);
  outputs.scene_out.dirty = ossia::scene_port::dirty_transform;
}

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
}

}

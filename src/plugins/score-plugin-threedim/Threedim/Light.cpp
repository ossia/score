#include "Light.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <cmath>
#include <cstring>

namespace Threedim
{

namespace
{
inline ossia::light_type toLightType(Light::Mode m) noexcept
{
  switch(m)
  {
    case Light::Directional: return ossia::light_type::directional;
    case Light::Point:       return ossia::light_type::point;
    case Light::Spot:        return ossia::light_type::spot;
    case Light::Rect:        return ossia::light_type::rect_area;
    case Light::Disk:        return ossia::light_type::disk_area;
    case Light::Sphere:      return ossia::light_type::sphere_area;
    case Light::Dome:        return ossia::light_type::dome;
  }
  return ossia::light_type::point;
}

inline ossia::light_decay toLightDecay(Light::Decay d) noexcept
{
  switch(d)
  {
    case Light::DecayNone:      return ossia::light_decay::none;
    case Light::DecayLinear:    return ossia::light_decay::linear;
    case Light::DecayQuadratic: return ossia::light_decay::quadratic;
    case Light::DecayCubic:     return ossia::light_decay::cubic;
  }
  return ossia::light_decay::quadratic;
}
}

void Light::rebuild()
{
  if(!m_state)
    m_state = std::make_shared<ossia::scene_state>();
  if(m_light_stable_id == 0)
    m_light_stable_id = ossia::mint_stable_id();
  if(m_xform_stable_id == 0)
    m_xform_stable_id = ossia::mint_stable_id();

  auto lc = std::make_shared<ossia::light_component>();
  lc->stable_id = m_light_stable_id;
  lc->type = toLightType(Mode(inputs.mode.value));
  lc->decay = toLightDecay(Decay(inputs.decay.value));

  lc->color[0] = inputs.color.value.r;
  lc->color[1] = inputs.color.value.g;
  lc->color[2] = inputs.color.value.b;
  lc->intensity = inputs.intensity.value;
  lc->range = inputs.range.value;

  // Degrees → radians for cone angles.
  constexpr float deg2rad = float(M_PI) / 180.f;
  lc->inner_cone_angle = inputs.inner_cone.value * deg2rad;
  lc->outer_cone_angle = inputs.outer_cone.value * deg2rad;

  // Area-shape dimensions: Rect uses width+height, Disk/Sphere use
  // radius. The fields are unused for Directional/Point/Spot but
  // setting them anyway is harmless.
  lc->width = inputs.width.value;
  lc->height = inputs.height.value;
  lc->radius = inputs.radius.value;

  lc->shadow.enabled = inputs.cast_shadow.value;
  lc->shadow.bias = inputs.shadow_bias.value;
  lc->shadow.normal_bias = inputs.shadow_normal_bias.value;

  // Propagate the RawLight arena slot ref (populated in init()).
  lc->raw_slot = m_light_ref;

  lc->dirty_index = ++m_version;

  // Standard wrapping: a scene_node holding [scene_transform,
  // light_component]. The transform encodes the light's world position
  // + orientation; FlattenVisitor pushes that through parentWorld when
  // visiting this node, so the light's direction column ends up
  // correctly oriented in world space even when the node is placed
  // under a parent transform chain.
  ossia::scene_transform xform;
  xform.stable_id = m_xform_stable_id;
  xform.translation[0] = inputs.position.value.x;
  xform.translation[1] = inputs.position.value.y;
  xform.translation[2] = inputs.position.value.z;

  QQuaternion q = QQuaternion::fromEulerAngles(
      inputs.rotation.value.x,
      inputs.rotation.value.y,
      inputs.rotation.value.z);

  // Directional / spot / area-light direction is determined by the
  // node's rotation applied to -Z (Vulkan / glTF convention). When
  // the rotation is identity, the light points along -Z.
  xform.rotation[0] = q.x();
  xform.rotation[1] = q.y();
  xform.rotation[2] = q.z();
  xform.rotation[3] = q.scalar();
  xform.scale[0] = xform.scale[1] = xform.scale[2] = 1.f;
  // Propagate the RawTransform slot ref (populated in init()).
  xform.raw_slot = m_xform_ref;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(xform);
  children->push_back(ossia::light_component_ptr(std::move(lc)));

  auto node = std::make_shared<ossia::scene_node>();
  node->children = std::move(children);

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(node));

  m_state->roots = std::move(roots);
  m_state->version = m_version;
  m_pending_dirty = ossia::scene_port::dirty_lights;
}

void Light::operator()()
{
  if(!m_state)
    rebuild();
  outputs.scene_out.scene.state = m_state;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

namespace
{
// Mode → raw type encoding used by RawLightData::local_direction.w and
// LightGPU::position_type.w. Area / dome modes collapse onto punctual
// analogues for the raw arena (directional for dome, point for rect /
// disk / sphere) — area-light shading is a shader-side extension
// scheduled after the preprocessor consumes the raw slots.
inline float toRawLightType(Light::Mode m) noexcept
{
  switch(m)
  {
    case Light::Directional: return 0.f;
    case Light::Point:       return 1.f;
    case Light::Spot:        return 2.f;
    case Light::Rect:
    case Light::Disk:
    case Light::Sphere:      return 1.f;
    case Light::Dome:        return 0.f;
  }
  return 1.f;
}

inline uint32_t toRawLightDecay(Light::Decay d) noexcept
{
  return (uint32_t)d;
}
}

// Order invariant: called by GfxRenderer::initState BEFORE the first
// operator()() and BEFORE processControlIn fires any rebuild() callback.
// m_light_ref / m_xform_ref populated here are therefore safe to read
// in rebuild() without a guard. Adding prepare() to this node breaks the
// invariant — see CpuFilterNode.hpp for details.
void Light::init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  if(!raw_light_slot.valid())
  {
    raw_light_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawLight,
        sizeof(score::gfx::RawLightData));
    m_light_ref = r.registry().toOssiaRef(raw_light_slot);
  }
  if(raw_light_slot.valid())
  {
    score::gfx::RawLightData seed{};
    r.registry().updateSlot(res, raw_light_slot, &seed, sizeof(seed));
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

void Light::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(!raw_light_slot.valid())
    return;

  score::gfx::RawLightData raw{};
  raw.color[0] = inputs.color.value.r;
  raw.color[1] = inputs.color.value.g;
  raw.color[2] = inputs.color.value.b;
  raw.color[3] = inputs.intensity.value;

  // Light convention: local -Z is the configured direction. The
  // preprocessor's world-matrix pass maps that through the node's
  // parent chain + rotation to get the world-space direction used
  // by the consumer shader. Keep the canonical local vector here.
  raw.local_direction[0] = 0.f;
  raw.local_direction[1] = 0.f;
  raw.local_direction[2] = -1.f;
  raw.local_direction[3] = toRawLightType(Mode(inputs.mode.value));

  constexpr float deg2rad = float(M_PI) / 180.f;
  const float inner_rad = inputs.inner_cone.value * deg2rad;
  const float outer_rad = inputs.outer_cone.value * deg2rad;

  raw.range_cone[0] = inputs.range.value;
  raw.range_cone[1] = std::cos(inner_rad);
  raw.range_cone[2] = std::cos(outer_rad);
  raw.range_cone[3] = inputs.shadow_bias.value;

  raw.shadow_enabled = inputs.cast_shadow.value ? 1u : 0u;
  raw.decay_mode = toRawLightDecay(Decay(inputs.decay.value));
  raw.normal_bias = inputs.shadow_normal_bias.value;
  // Stamp our scene_transform's arena slot index so consumer shaders
  // can read world_transforms.data[transform_slot] to compose a world-
  // space direction/position from the local-frame fields above.
  raw.transform_slot = raw_transform_slot.valid()
                           ? raw_transform_slot.slot_index
                           : 0u;

  r.registry().updateSlot(res, raw_light_slot, &raw, sizeof(raw));

  if(raw_transform_slot.valid())
  {
    score::gfx::RawLocalTransform xform{};
    xform.translation[0] = inputs.position.value.x;
    xform.translation[1] = inputs.position.value.y;
    xform.translation[2] = inputs.position.value.z;
    QQuaternion q = QQuaternion::fromEulerAngles(
        inputs.rotation.value.x, inputs.rotation.value.y,
        inputs.rotation.value.z);
    xform.rotation[0] = q.x();
    xform.rotation[1] = q.y();
    xform.rotation[2] = q.z();
    xform.rotation[3] = q.scalar();
    xform.scale[0] = 1.f;
    xform.scale[1] = 1.f;
    xform.scale[2] = 1.f;
    r.registry().updateSlot(res, raw_transform_slot, &xform, sizeof(xform));
  }
}

void Light::release(score::gfx::RenderList& r)
{
  if(raw_light_slot.valid())
    r.registry().free(raw_light_slot);
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_light_ref = {};
  m_xform_ref = {};
  // Clear the cached scene_state shared_ptr so the next operator()()
  // re-runs rebuild() against the post-release registry. Without this,
  // an in-place release+init path (relinkGraph / maybeRebuild) would
  // republish a state whose lc->raw_slot still embeds the OLD
  // (now-freed) slot index. ScenePreprocessor then harvests that
  // stale index into scene_light_indices, the rasterizer reads from
  // a different slot than the one Light::update() is now writing
  // to → wildly wrong lighting that drifts each cycle as the LIFO
  // free-list reshuffles. Producer-state-drift Option A.
  m_state.reset();
}

} // namespace Threedim

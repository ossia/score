#include "SceneGroup.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <algorithm>
#include <cstring>

namespace Threedim
{

namespace
{
// Concatenate a shared vector from two nullable inputs. Reuses the
// lone non-null input's shared_ptr when only one contributes — the
// same identity-preserving passthrough merge_scenes does.
template <typename T>
std::shared_ptr<const std::vector<T>> mergeSharedVec(
    const std::shared_ptr<const std::vector<T>>& a,
    const std::shared_ptr<const std::vector<T>>& b)
{
  if(!a || a->empty())
    return b;
  if(!b || b->empty())
    return a;
  auto merged = std::make_shared<std::vector<T>>();
  merged->reserve(a->size() + b->size());
  for(const auto& x : *a)
    merged->push_back(x);
  for(const auto& x : *b)
    merged->push_back(x);
  return merged;
}
} // namespace

void SceneGroup::rebuild()
{
  const ossia::scene_spec* inputs_list[4] = {
      &inputs.scene0.scene, &inputs.scene1.scene,
      &inputs.scene2.scene, &inputs.scene3.scene};

  // Refresh upstream identity cache (used by operator()() to detect
  // changes) and TRS / name caches.
  for(int i = 0; i < 4; ++i)
  {
    const ossia::scene_state* s = inputs_list[i]->state.get();
    int64_t v = s ? s->version : -1;
    m_cached_in[i] = s;
    m_cached_ver[i] = v;
  }
  m_cached_name = inputs.name.value;
  float scratch[16];
  CachedTRS xcache = m_cachedTRS;
  computeTRSMatrix(inputs, scratch, xcache);
  m_cachedTRS = xcache;

  // Collect roots from all non-empty inputs; also concat materials /
  // animations / cameras / skeletons additively.
  auto merged_roots
      = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  std::shared_ptr<const std::vector<ossia::material_component_ptr>> mats;
  std::shared_ptr<const std::vector<ossia::animation_component_ptr>> anims;
  std::shared_ptr<const std::vector<ossia::camera_component_ptr>> cams;
  std::shared_ptr<const std::vector<ossia::skeleton_component_ptr>> skels;
  ossia::scene_environment env{};
  ossia::scene_node_id active_cam{};

  for(int i = 0; i < 4; ++i)
  {
    const auto& s = inputs_list[i]->state;
    if(!s)
      continue;
    if(s->roots)
      for(const auto& r : *s->roots)
        merged_roots->push_back(r);
    mats = mergeSharedVec(mats, s->materials);
    anims = mergeSharedVec(anims, s->animations);
    cams = mergeSharedVec(cams, s->cameras);
    skels = mergeSharedVec(skels, s->skeletons);
    // First contributor's environment + active_camera wins.
    if(i == 0 || !env.skybox_texture.native_handle)
      env = s->environment;
    if(active_cam.value == 0 && s->active_camera_id.value != 0)
      active_cam = s->active_camera_id;
  }

  // Build the wrapping parent node.
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
  xform.raw_slot = m_xform_ref;

  auto children
      = std::make_shared<std::vector<ossia::scene_payload>>();
  children->reserve(merged_roots->size() + 1);
  children->push_back(xform);
  for(auto& r : *merged_roots)
    children->push_back(r);

  auto parent = std::make_shared<ossia::scene_node>();
  parent->name
      = inputs.name.value.empty() ? std::string{"Group"}
                                  : inputs.name.value;
  parent->children = std::move(children);

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(parent));

  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::move(roots);
  state->materials = std::move(mats);
  state->animations = std::move(anims);
  state->cameras = std::move(cams);
  state->skeletons = std::move(skels);
  state->environment = std::move(env);
  state->active_camera_id = active_cam;
  state->version = ++m_version_counter;
  state->dirty_index = 1;

  m_cached_out = std::move(state);
  m_pending_dirty = 0xFF;
}

void SceneGroup::operator()()
{
  // Detect upstream scene inputs + republish cached. Control changes
  // come through their update() callbacks.
  const ossia::scene_spec* inputs_list[4] = {
      &inputs.scene0.scene, &inputs.scene1.scene,
      &inputs.scene2.scene, &inputs.scene3.scene};
  bool upstream_changed = false;
  for(int i = 0; i < 4; ++i)
  {
    const auto* s = inputs_list[i]->state.get();
    const int64_t v = s ? s->version : -1;
    if(m_cached_in[i] != s || m_cached_ver[i] != v)
      upstream_changed = true;
  }
  if(!m_cached_out || upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

void SceneGroup::init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
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

void SceneGroup::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(!raw_transform_slot.valid())
    return;

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
  xform.scale[0] = inputs.scale.value.x;
  xform.scale[1] = inputs.scale.value.y;
  xform.scale[2] = inputs.scale.value.z;
  r.registry().updateSlot(res, raw_transform_slot, &xform, sizeof(xform));
}

void SceneGroup::release(score::gfx::RenderList& r)
{
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_xform_ref = {};
}

} // namespace Threedim

#include "SceneDuplicator.hpp"

#include <QQuaternion>

#include <cmath>

namespace Threedim
{

namespace
{

// Compute one clone's TRS given its index and the pattern / params.
// Fills an ossia::scene_transform suitable for prepending to the cloned
// root's children. All positions in world space; parents identity.
ossia::scene_transform
transformForIndex(int idx, int count, int mode, int grid_cols_hint,
                  float spacing, float radius) noexcept
{
  ossia::scene_transform t;
  t.rotation[3] = 1.f;
  t.scale[0] = t.scale[1] = t.scale[2] = 1.f;

  switch(mode)
  {
    case SceneDuplicator::Grid:
    {
      const int cols = grid_cols_hint > 0
          ? grid_cols_hint
          : std::max(1, (int)std::round(std::sqrt(double(count))));
      const int row = idx / cols;
      const int col = idx % cols;
      // Center the grid around the origin.
      const int rows = (count + cols - 1) / cols;
      const float cx = (col - 0.5f * (cols - 1)) * spacing;
      const float cz = (row - 0.5f * (rows - 1)) * spacing;
      t.translation[0] = cx;
      t.translation[1] = 0.f;
      t.translation[2] = cz;
      break;
    }
    case SceneDuplicator::Ring:
    {
      const float theta = (count > 0)
          ? (float(idx) / float(count)) * 2.f * float(M_PI)
          : 0.f;
      t.translation[0] = radius * std::cos(theta);
      t.translation[1] = 0.f;
      t.translation[2] = radius * std::sin(theta);
      // Face outward (local +Z towards the center). Rotate around Y so
      // local -Z points away from the origin.
      auto q = QQuaternion::fromEulerAngles(
          0.f, -theta * 180.f / float(M_PI), 0.f);
      t.rotation[0] = q.x();
      t.rotation[1] = q.y();
      t.rotation[2] = q.z();
      t.rotation[3] = q.scalar();
      break;
    }
    case SceneDuplicator::Line:
    default:
    {
      t.translation[0] = (idx - 0.5f * (count - 1)) * spacing;
      t.translation[1] = 0.f;
      t.translation[2] = 0.f;
      break;
    }
  }
  return t;
}

// Build one cloned root scene_node wrapping the prototype's roots.
// Structure:
//   scene_node { name = "<base>_<idx>", children = [
//       scene_transform(xform),
//       ...prototype roots (as scene_node_ptr payloads — shared; cheap)
//   ]}
ossia::scene_node_ptr makeCloneRoot(
    const std::vector<ossia::scene_node_ptr>& proto_roots,
    const std::string& base_name, int idx,
    const ossia::scene_transform& xform, int64_t dirty_index)
{
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->reserve(1 + proto_roots.size());
  children->push_back(xform);
  for(const auto& r : proto_roots)
    if(r)
      children->push_back(r);

  auto node = std::make_shared<ossia::scene_node>();
  node->name = base_name + "_" + std::to_string(idx);
  node->children = std::move(children);
  node->dirty_index = dirty_index;
  return node;
}

} // namespace

void SceneDuplicator::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  const int count = std::max(1, inputs.count.value);

  m_cached_in_state = in_state;
  m_cached_in_version = in_version;

  if(!in_state || !in_state->roots || in_state->roots->empty())
  {
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    return;
  }

  // Base name for clones — derived from the first root's name, falling
  // back to "Clone" when the prototype has no names.
  std::string base = (*in_state->roots)[0] ? (*in_state->roots)[0]->name
                                           : std::string{};
  if(base.empty())
    base = "Clone";

  const int64_t version = ++m_version_counter;

  auto new_roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  new_roots->reserve(count);
  for(int i = 0; i < count; ++i)
  {
    const auto xform = transformForIndex(
        i, count, inputs.pattern.value, inputs.grid_cols.value,
        inputs.spacing.value, inputs.radius.value);
    new_roots->push_back(
        makeCloneRoot(*in_state->roots, base, i, xform, version));
  }

  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::move(new_roots);
  // Share all non-root resources with the input — clones read the same
  // materials / animations / cameras / skeletons / environment.
  state->materials = in_state->materials;
  state->animations = in_state->animations;
  state->cameras = in_state->cameras;
  state->skeletons = in_state->skeletons;
  state->environment = in_state->environment;
  state->active_camera_id = in_state->active_camera_id;
  state->version = version;
  state->dirty_index = version;

  m_cached_out = state;
  m_pending_dirty = 0xFF;
}

void SceneDuplicator::operator()()
{
  const auto* in_state = inputs.scene_in.scene.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  const bool upstream_changed
      = m_cached_in_state != in_state || m_cached_in_version != in_version;
  if(!m_cached_out || upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim

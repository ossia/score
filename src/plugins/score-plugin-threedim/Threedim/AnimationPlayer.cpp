#include "AnimationPlayer.hpp"

#include <QMatrix4x4>
#include <QQuaternion>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Threedim
{

namespace
{

// Accumulated TRS override for a single scene_node. Any missing field
// (has_* == false) leaves the original value untouched.
struct TRSOverride
{
  float translation[3]{};
  float rotation[4]{};  // quaternion x,y,z,w
  float scale[3]{};
  bool has_translation{false};
  bool has_rotation{false};
  bool has_scale{false};
};

using TRSMap = std::unordered_map<uint64_t, TRSOverride>;

// Binary-search for the segment `[times[i], times[i+1]]` that contains `t`.
// Returns (i, alpha) with alpha ∈ [0, 1). For t at or after the last
// keyframe, returns (n-2, 1) so the caller lands on the final value.
struct SegmentLookup
{
  std::size_t lower{};
  float alpha{};
};

SegmentLookup
findSegment(const std::vector<float>& times, float t) noexcept
{
  const std::size_t n = times.size();
  if(n == 0)
    return {0, 0.f};
  if(n == 1 || t <= times.front())
    return {0, 0.f};
  if(t >= times.back())
    return {n - 1, 1.f}; // alpha unused in the lerp when clamped below

  // std::upper_bound finds the first key > t → segment is its left neighbour.
  auto it = std::upper_bound(times.begin(), times.end(), t);
  const std::size_t upper = std::size_t(it - times.begin());
  const std::size_t lower = upper - 1;
  const float t0 = times[lower];
  const float t1 = times[upper];
  const float span = t1 - t0;
  const float alpha = span > 1e-8f ? (t - t0) / span : 0.f;
  return {lower, alpha};
}

// Lerp for scalars / vec3 / vec4 depending on `stride`. Step and linear
// covered; cubic_spline is treated as linear for this first pass (proper
// cubic_spline keyframes pack `in-tangent, value, out-tangent` per slot
// at 3× stride — handling it right requires knowing the format, added
// later).
void sampleLinear(
    const std::vector<float>& values, std::size_t stride, SegmentLookup s,
    float* out) noexcept
{
  const std::size_t n = values.size() / stride;
  if(n == 0)
    return;
  if(s.lower >= n - 1 || s.alpha <= 0.f)
  {
    const std::size_t idx = std::min(s.lower, n - 1);
    std::memcpy(out, values.data() + idx * stride, stride * sizeof(float));
    return;
  }
  const float* a = values.data() + s.lower * stride;
  const float* b = values.data() + (s.lower + 1) * stride;
  const float alpha = s.alpha;
  for(std::size_t i = 0; i < stride; ++i)
    out[i] = a[i] + (b[i] - a[i]) * alpha;
}

// Quaternion slerp via QQuaternion — handles shortest-arc vs. double-cover.
void sampleSlerp(
    const std::vector<float>& values, SegmentLookup s, float out[4]) noexcept
{
  const std::size_t n = values.size() / 4;
  if(n == 0)
    return;
  if(s.lower >= n - 1 || s.alpha <= 0.f)
  {
    const std::size_t idx = std::min(s.lower, n - 1);
    std::memcpy(out, values.data() + idx * 4, 4 * sizeof(float));
    return;
  }
  const float* a = values.data() + s.lower * 4;
  const float* b = values.data() + (s.lower + 1) * 4;
  // glTF convention: (x, y, z, w). QQuaternion uses (scalar, x, y, z).
  QQuaternion qa(a[3], a[0], a[1], a[2]);
  QQuaternion qb(b[3], b[0], b[1], b[2]);
  QQuaternion r = QQuaternion::slerp(qa, qb, s.alpha).normalized();
  out[0] = r.x();
  out[1] = r.y();
  out[2] = r.z();
  out[3] = r.scalar();
}

// Walk the raw scene tree and emit a cloned subtree with overrides
// applied. Subtrees that contain no animated node are returned as the
// same shared_ptr (structural sharing) so downstream caches see
// unchanged pointers for the un-animated branches.
struct CloneVisitor
{
  const TRSMap& overrides;

  // Recursive scan: is any descendant (including this node) animated?
  // Result cached per-visit via the node identity — quick
  // short-circuit since scene trees are typically shallow.
  bool subtree_is_animated(const ossia::scene_node& n) const noexcept
  {
    if(overrides.find(n.id.value) != overrides.end())
      return true;
    if(!n.has_children())
      return false;
    for(const auto& child : *n.children)
    {
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&child))
        if(*sub && subtree_is_animated(**sub))
          return true;
    }
    return false;
  }

  ossia::scene_node_ptr clone(const ossia::scene_node_ptr& orig) const
  {
    if(!orig)
      return orig;
    if(!subtree_is_animated(*orig))
      return orig; // whole subtree unchanged → share

    auto new_node = std::make_shared<ossia::scene_node>(*orig);
    std::vector<ossia::scene_payload> new_children;
    if(orig->children)
      new_children.reserve(orig->children->size());

    bool xform_replaced = false;
    auto it = overrides.find(orig->id.value);
    const auto* ov = it != overrides.end() ? &it->second : nullptr;

    if(orig->children)
    {
      for(const auto& payload : *orig->children)
      {
        if(ov && !xform_replaced)
        {
          if(auto* xf = ossia::get_if<ossia::scene_transform>(&payload))
          {
            // Override the first scene_transform we encounter in this
            // node's children (GltfParser / FbxParser convention:
            // they prepend one as the first child of each node).
            ossia::scene_transform merged = *xf;
            if(ov->has_translation)
              std::memcpy(merged.translation, ov->translation, 12);
            if(ov->has_rotation)
              std::memcpy(merged.rotation, ov->rotation, 16);
            if(ov->has_scale)
              std::memcpy(merged.scale, ov->scale, 12);
            new_children.push_back(merged);
            xform_replaced = true;
            continue;
          }
        }

        // Recurse into sub-scene_node payloads so descendants can
        // also be animated.
        if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&payload))
        {
          new_children.push_back(clone(*sub));
          continue;
        }

        new_children.push_back(payload);
      }
    }

    // If this node is animated but had no scene_transform child, insert
    // one at the start so the TRS takes effect on subsequent siblings.
    if(ov && !xform_replaced)
    {
      ossia::scene_transform inserted{};
      inserted.rotation[3] = 1.f; // identity quaternion w
      inserted.scale[0] = inserted.scale[1] = inserted.scale[2] = 1.f;
      if(ov->has_translation)
        std::memcpy(inserted.translation, ov->translation, 12);
      if(ov->has_rotation)
        std::memcpy(inserted.rotation, ov->rotation, 16);
      if(ov->has_scale)
        std::memcpy(inserted.scale, ov->scale, 12);
      new_children.insert(new_children.begin(), inserted);
    }

    new_node->children
        = std::make_shared<const std::vector<ossia::scene_payload>>(
            std::move(new_children));
    new_node->dirty_index = orig->dirty_index + 1;
    return new_node;
  }
};

} // namespace

void AnimationPlayer::operator()()
{
  const auto& in = inputs.scene_in.scene;
  if(!in.state || in.state->empty() || !in.state->animations
     || in.state->animations->empty())
  {
    outputs.scene_out.scene = in;
    outputs.scene_out.dirty = 0;
    return;
  }

  float t = inputs.time.value;
  // The speed control contributes purely additive offset between
  // consecutive calls so users who wire only the Time inlet get
  // unmodified behavior. If the user leaves Time at 0 and moves Speed,
  // we integrate Speed over frame-delta (approximated as 1/60 s per
  // call — halp doesn't expose a deterministic dt yet).
  const float speed = inputs.speed.value;
  if(t == m_prev_time && speed != 1.f && speed != 0.f)
    t = m_prev_time + speed * (1.f / 60.f);
  m_prev_time = t;

  // Collect animation_components to sample.
  const auto& anims = *in.state->animations;
  const int clip_i = inputs.clip_index.value;
  std::vector<const ossia::animation_component*> clips;
  clips.reserve(anims.size());
  if(clip_i < 0)
  {
    for(const auto& a : anims)
      if(a)
        clips.push_back(a.get());
  }
  else if(std::size_t(clip_i) < anims.size() && anims[clip_i])
  {
    clips.push_back(anims[clip_i].get());
  }

  TRSMap overrides;
  for(const auto* clip : clips)
  {
    float clip_t = t;
    if(inputs.loop.value && clip->duration > 0.f)
    {
      // Modulo into [0, duration). std::fmod preserves sign; add and
      // modulo again for negative t (caused by negative speed).
      clip_t = std::fmod(t, clip->duration);
      if(clip_t < 0.f)
        clip_t += clip->duration;
    }
    else if(clip->duration > 0.f)
    {
      clip_t = std::clamp(clip_t, 0.f, clip->duration);
    }

    for(const auto& channel : clip->channels)
    {
      if(!channel.times || !channel.values)
        continue;
      const auto& times = *channel.times;
      const auto& values = *channel.values;
      auto seg = findSegment(times, clip_t);

      auto& ov = overrides[channel.target_node_id];
      switch(channel.target_path)
      {
        case ossia::animation_target::translation: {
          sampleLinear(values, 3, seg, ov.translation);
          ov.has_translation = true;
          break;
        }
        case ossia::animation_target::rotation: {
          sampleSlerp(values, seg, ov.rotation);
          ov.has_rotation = true;
          break;
        }
        case ossia::animation_target::scale: {
          sampleLinear(values, 3, seg, ov.scale);
          ov.has_scale = true;
          break;
        }
        default:
          // weights / custom — deliberately ignored; see header comment.
          break;
      }
    }
  }

  if(overrides.empty())
  {
    // No channels matched anything at this time (e.g., empty keyframe
    // arrays). Pass through without bumping version.
    outputs.scene_out.scene = in;
    outputs.scene_out.dirty = 0;
    return;
  }

  // Clone-and-override the tree.
  CloneVisitor vis{overrides};
  auto new_roots
      = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  if(in.state->roots)
  {
    new_roots->reserve(in.state->roots->size());
    for(const auto& r : *in.state->roots)
      new_roots->push_back(vis.clone(r));
  }

  auto new_state = std::make_shared<ossia::scene_state>(*in.state);
  new_state->roots = new_roots;
  new_state->version = ++m_version_counter;
  new_state->dirty_index = in.state->dirty_index + 1;

  // ── Skinning update ──────────────────────────────────────────────
  // Skinned meshes are deformed by the renderer's forward kinematics
  // over skeleton_component::joints[].{translation,rotation,scale}
  // (SceneGPUState.cpp packs joint_matrix[i] = FK(joints)[i] ×
  // inverse_bind[i]). glTF joint tracks target the joints' backing
  // scene_nodes, so the overrides sampled above (keyed by scene_node id)
  // are precisely each joint's new *local* TRS. Map joint j back to its
  // node via joint_node_ids[j] and write the override into the cloned
  // skeleton's joints[], then bump dirty_index so the renderer re-runs
  // FK against the animated pose.
  //
  // NOTE: joint_matrices_buffer is intentionally left untouched — no
  // skinning consumer reads it (verified across src/ + libossia); the
  // joints[] route is the one the actual consumer (SceneGPUState) uses.
  if(in.state->skeletons && !in.state->skeletons->empty())
  {
    auto new_skels
        = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
    new_skels->reserve(in.state->skeletons->size());
    for(const auto& src : *in.state->skeletons)
    {
      if(!src)
      {
        new_skels->push_back(src);
        continue;
      }

      auto cloned = std::make_shared<ossia::skeleton_component>(*src);
      const std::size_t n
          = std::min(cloned->joints.size(), cloned->joint_node_ids.size());
      bool any = false;
      for(std::size_t j = 0; j < n; ++j)
      {
        auto it = overrides.find(cloned->joint_node_ids[j].value);
        if(it == overrides.end())
          continue;
        const TRSOverride& ov = it->second;
        // Only overwrite the animated components; leave the bind-pose
        // value for channels the clip doesn't drive.
        if(ov.has_translation)
          std::memcpy(cloned->joints[j].translation, ov.translation, 12);
        if(ov.has_rotation)
          std::memcpy(cloned->joints[j].rotation, ov.rotation, 16);
        if(ov.has_scale)
          std::memcpy(cloned->joints[j].scale, ov.scale, 12);
        any = true;
      }
      if(any)
        cloned->dirty_index = new_state->version;
      new_skels->push_back(std::move(cloned));
    }
    new_state->skeletons = std::move(new_skels);
  }

  outputs.scene_out.scene.state = std::move(new_state);
  outputs.scene_out.dirty = ossia::scene_port::dirty_animation;
}

} // namespace Threedim

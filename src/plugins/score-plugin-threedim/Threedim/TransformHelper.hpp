#pragma once
#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>
#include <QQuaternion>

#include <cstring>
#include <memory>
#include <vector>

namespace Threedim
{

// Shared TRS-matrix computation for halp nodes that output a
// `halp::mesh`-style geometry with a `transform[16]` slot plus a
// `dirty_transform` flag (BuffersToGeometry, BuffersToGeometry2,
// VoxelLoader, ...). Call from operator() every frame: computes a
// column-major 4x4 TRS matrix from the XYZ controls, writes it into
// `out_transform16`, and returns true iff the matrix changed since
// the last call (so the caller can set `dirty_transform` accordingly).
//
// Cached prev values live on the caller via the CachedTRS struct —
// identical layout across the three call sites so each node just
// declares one member `CachedTRS m_cachedTRS{}` and passes it in.
struct CachedTRS
{
  float pos[3]{0, 0, 0};
  float rot[3]{0, 0, 0};
  float scale[3]{1, 1, 1};
  bool valid{false};
};

// `Inputs` is duck-typed: must expose `.position.value.{x,y,z}`, etc.
template <typename Inputs>
inline bool
computeTRSMatrix(const Inputs& inputs, float out_transform16[16], CachedTRS& cache)
{
  const float px = inputs.position.value.x;
  const float py = inputs.position.value.y;
  const float pz = inputs.position.value.z;
  const float rx = inputs.rotation.value.x;
  const float ry = inputs.rotation.value.y;
  const float rz = inputs.rotation.value.z;
  const float sx = inputs.scale.value.x;
  const float sy = inputs.scale.value.y;
  const float sz = inputs.scale.value.z;

  const bool changed
      = !cache.valid
        || cache.pos[0] != px || cache.pos[1] != py || cache.pos[2] != pz
        || cache.rot[0] != rx || cache.rot[1] != ry || cache.rot[2] != rz
        || cache.scale[0] != sx || cache.scale[1] != sy || cache.scale[2] != sz;

  if(!changed)
    return false;

  // Build column-major 4x4: translate * rotate * scale, matching the
  // convention used across the 3D plugin (QMatrix4x4's constData()
  // returns column-major).
  QMatrix4x4 m;
  m.translate(px, py, pz);
  m.rotate(QQuaternion::fromEulerAngles(rx, ry, rz));
  m.scale(sx, sy, sz);
  std::memcpy(out_transform16, m.constData(), sizeof(float) * 16);

  cache.pos[0] = px; cache.pos[1] = py; cache.pos[2] = pz;
  cache.rot[0] = rx; cache.rot[1] = ry; cache.rot[2] = rz;
  cache.scale[0] = sx; cache.scale[1] = sy; cache.scale[2] = sz;
  cache.valid = true;
  return true;
}

// Wrap a raw scene_state under a single parent scene_node whose first child
// is a scene_transform carrying this node's position / rotation / scale
// controls. FlattenVisitor processes payloads in order and transforms apply
// to subsequent siblings, so the wrap applies the TRS to every descendant.
//
// Used by asset-loader-style nodes (FbxParser, GltfParser, AssetLoader) to
// compose the control-knob transform on top of the as-loaded scene without
// touching the raw state (kept stable so downstream identity caches stay
// warm). Shared to avoid re-duplicating the same 40 lines per loader.
//
// `Inputs` is duck-typed: must expose `.position.value.{x,y,z}`,
// `.rotation.value.{x,y,z}`, `.scale.value.{x,y,z}`.
template <typename Inputs>
inline std::shared_ptr<const ossia::scene_state> wrapSceneWithTransform(
    const std::shared_ptr<const ossia::scene_state>& raw,
    const Inputs& inputs, CachedTRS& cache, int64_t& version_counter,
    const ossia::gpu_slot_ref& xform_ref = {})
{
  if(!raw)
    return nullptr;

  // Skip rebuild when nothing changed: cache check also updates the cache
  // on a real change. We rebuild when there IS no wrapped output yet (first
  // call) OR when inputs differ from the cache; compute cache-hit separately.
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
  // Stamp the producer's RawTransform slot ref (if any) so the
  // preprocessor composes a world matrix at the matching offset.
  xform.raw_slot = xform_ref;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(xform);
  if(raw->roots)
    for(const auto& root : *raw->roots)
      children->push_back(root);

  auto parent = std::make_shared<ossia::scene_node>();
  parent->children = std::move(children);

  auto new_roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  new_roots->push_back(std::move(parent));

  auto wrapped = std::make_shared<ossia::scene_state>();
  wrapped->roots       = std::move(new_roots);
  // Identity-preserving passthrough of every scene_state shared field so
  // downstream caches stay warm. `collections` was missed in the initial
  // landing (CreateCollection writes them onto scene_state::collections,
  // and dropping them here silently loses the named-collection list on
  // every TRS pass) — diagnostic 026.
  wrapped->materials        = raw->materials;
  wrapped->animations       = raw->animations;
  wrapped->cameras          = raw->cameras;
  wrapped->skeletons        = raw->skeletons;
  wrapped->collections      = raw->collections;
  wrapped->environment      = raw->environment;
  // Same diagnostic-026 bug class for the newer fields: a Transform3D
  // downstream of ShadowCascadeSetup (or of buffer/texture injectors)
  // must not zero these out.
  wrapped->shadow_cascades      = raw->shadow_cascades;
  wrapped->inject_buffers       = raw->inject_buffers;
  wrapped->inject_textures      = raw->inject_textures;
  wrapped->time_seconds         = raw->time_seconds;
  wrapped->active_variant_index = raw->active_variant_index;
  wrapped->variant_names        = raw->variant_names;
  wrapped->active_camera_id = raw->active_camera_id;
  wrapped->version          = ++version_counter;
  wrapped->dirty_index      = 1;

  cache.pos[0]   = inputs.position.value.x;
  cache.pos[1]   = inputs.position.value.y;
  cache.pos[2]   = inputs.position.value.z;
  cache.rot[0]   = inputs.rotation.value.x;
  cache.rot[1]   = inputs.rotation.value.y;
  cache.rot[2]   = inputs.rotation.value.z;
  cache.scale[0] = inputs.scale.value.x;
  cache.scale[1] = inputs.scale.value.y;
  cache.scale[2] = inputs.scale.value.z;
  cache.valid    = true;

  return wrapped;
}

// Test whether the controls differ from a prior cached snapshot, without
// applying them. Use this to gate a wrapSceneWithTransform() rebuild when
// you want to only allocate a new wrapped state when the user moved a knob.
template <typename Inputs>
inline bool transformChanged(const Inputs& inputs, const CachedTRS& cache)
{
  return !cache.valid
      || cache.pos[0] != inputs.position.value.x
      || cache.pos[1] != inputs.position.value.y
      || cache.pos[2] != inputs.position.value.z
      || cache.rot[0] != inputs.rotation.value.x
      || cache.rot[1] != inputs.rotation.value.y
      || cache.rot[2] != inputs.rotation.value.z
      || cache.scale[0] != inputs.scale.value.x
      || cache.scale[1] != inputs.scale.value.y
      || cache.scale[2] != inputs.scale.value.z;
}

}

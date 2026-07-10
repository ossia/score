#include "Instancer.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <algorithm>
#include <cstring>

namespace Threedim
{

namespace
{

// Extract the first mesh_component found in a scene tree (depth-first),
// alongside the accumulated `scene_transform` composition encountered
// along the path from `node` to that mesh. The composition is what
// upstream producers use to position their meshes (a glTF root node's
// scale, a Primitive's TRS, etc.); without it, instancing a Duck.gltf
// would draw at the model's intrinsic origin / scale even when the
// upstream node was visibly scaled by the user.
//
// Two behaviours intentionally preserved:
//   - First-mesh-only: subtree may contain many meshes; only the first
//     in depth-first order is instanced. (The "instance all meshes"
//     combobox mode is a future feature.)
//   - Sibling scene_transforms BEFORE the mesh ARE composed (matches
//     the FlattenVisitor's "transform applies to subsequent siblings"
//     contract). Sibling transforms AFTER the mesh would only affect
//     later siblings and are correctly ignored here.
struct PrototypeWithTransform
{
  ossia::mesh_component_ptr mesh;
  QMatrix4x4                world;  // accumulated TRS from `node` down to `mesh`
};

namespace
{
QMatrix4x4 transformToMatrix(const ossia::scene_transform& t) noexcept
{
  QMatrix4x4 m;
  m.setToIdentity();
  m.translate(t.translation[0], t.translation[1], t.translation[2]);
  m.rotate(QQuaternion(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]));
  m.scale(t.scale[0], t.scale[1], t.scale[2]);
  return m;
}
}

PrototypeWithTransform
findFirstMesh(const ossia::scene_node& node, QMatrix4x4 parent = QMatrix4x4{}) noexcept
{
  PrototypeWithTransform out{nullptr, parent};
  if(!node.has_children())
    return out;

  QMatrix4x4 acc = parent;
  for(const auto& payload : *node.children)
  {
    // scene_transform among siblings updates the running composition
    // for any subsequent sibling — matching the FlattenVisitor's
    // semantics. (See SceneGPUState.cpp:visitPayload scene_transform
    // branch.)
    if(auto* xform = ossia::get_if<ossia::scene_transform>(&payload))
    {
      acc = acc * transformToMatrix(*xform);
      continue;
    }

    if(auto* m = ossia::get_if<ossia::mesh_component_ptr>(&payload))
    {
      if(*m)
      {
        out.mesh = *m;
        out.world = acc;
        return out;
      }
    }
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&payload))
    {
      if(*sub)
      {
        auto found = findFirstMesh(**sub, acc);
        if(found.mesh)
          return found;
      }
    }
  }
  return out;
}

// Wrap a halp::gpu_buffer (a thin {handle, byte_size, byte_offset}
// struct) into an ossia::buffer_resource_ptr carrying a
// gpu_buffer_handle variant. Returns null when the input handle is
// null (e.g., no edge wired into that port), letting callers skip
// that slot.
ossia::buffer_resource_ptr
wrapGpuBuffer(const halp::gpu_buffer& buf) noexcept
{
  if(!buf.handle)
    return nullptr;
  ossia::gpu_buffer_handle gh;
  gh.native_handle = buf.handle;
  gh.byte_size = buf.byte_size;
  gh.byte_offset = buf.byte_offset;
  auto res = std::make_shared<ossia::buffer_resource>();
  res->resource = gh;
  res->dirty_index = 1;
  return res;
}

// Result of walking a halp::dynamic_gpu_geometry for the attributes
// Instancer knows how to consume. Any slot without a matching
// attribute stays null and falls back to the raw buffer inputs.
struct PointCloudRouting
{
  ossia::buffer_resource_ptr transforms; // translation or transform_matrix
  ossia::buffer_resource_ptr colors;     // color0
  bool has_matrix{false};                // true if transform_matrix found
  int instance_count{-1};                // geometry.vertices, or -1
};

// Resolve a geometry attribute to its source {handle, byte_offset}
// by chasing attribute → input[binding] → buffers[input.buffer]. The
// byte offsets in the attribute and the input add; the final byte
// offset lives on the wrapped buffer_resource.
ossia::buffer_resource_ptr
wrapAttributeAsBuffer(const halp::dynamic_gpu_geometry& mesh,
                      const halp::geometry_attribute& attr) noexcept
{
  if(attr.binding < 0 || attr.binding >= (int)mesh.input.size())
    return nullptr;
  const auto& in = mesh.input[attr.binding];
  if(in.buffer < 0 || in.buffer >= (int)mesh.buffers.size())
    return nullptr;
  const auto& b = mesh.buffers[in.buffer];
  if(!b.handle)
    return nullptr;
  ossia::gpu_buffer_handle gh;
  gh.native_handle = b.handle;
  gh.byte_size = b.byte_size;
  gh.byte_offset = in.byte_offset + attr.byte_offset;
  auto res = std::make_shared<ossia::buffer_resource>();
  res->resource = gh;
  res->dirty_index = 1;
  return res;
}

PointCloudRouting extractPointCloud(
    const halp::dynamic_gpu_geometry& mesh) noexcept
{
  PointCloudRouting out;
  if(mesh.buffers.empty() || mesh.attributes.empty())
    return out;
  for(const auto& attr : mesh.attributes)
  {
    using S = halp::attribute_semantic;
    switch(attr.semantic)
    {
      // transform_matrix takes precedence over translation/position
      // because it carries the full TRS.
      case S::transform_matrix:
        out.transforms = wrapAttributeAsBuffer(mesh, attr);
        out.has_matrix = true;
        break;
      case S::translation:
      case S::position:
        if(!out.has_matrix && !out.transforms)
          out.transforms = wrapAttributeAsBuffer(mesh, attr);
        break;
      case S::color0:
        if(!out.colors)
          out.colors = wrapAttributeAsBuffer(mesh, attr);
        break;
      default:
        break;
    }
  }
  out.instance_count = mesh.vertices;
  return out;
}

} // namespace

void Instancer::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();

  // Find the prototype mesh in the incoming scene, alongside the
  // composed scene_transform from each ancestor walked along the way.
  // The composed transform feeds into the wrapped scene_node below
  // so the instance cloud honours the upstream's authored TRS (e.g.
  // a Primitive node's scale, a glTF root's positioning) rather than
  // dropping it on extraction.
  ossia::mesh_component_ptr proto;
  QMatrix4x4 protoWorld;
  protoWorld.setToIdentity();
  if(in.state && in.state->roots)
  {
    for(const auto& r : *in.state->roots)
    {
      if(!r)
        continue;
      auto found = findFirstMesh(*r);
      if(found.mesh)
      {
        proto = found.mesh;
        protoWorld = found.world;
        break;
      }
    }
  }

  // Point-cloud input takes precedence over the raw buffer inlets
  // when it's wired. We detect "wired" as "at least one buffer with
  // a non-null handle in the points mesh". The routing struct
  // populates transforms / colors from the matching attribute
  // semantics; empty routing falls back to the raw buffer ports.
  const bool has_points_input
      = !inputs.points.mesh.buffers.empty()
        && std::any_of(
               inputs.points.mesh.buffers.begin(),
               inputs.points.mesh.buffers.end(),
               [](const halp::geometry_gpu_buffer& b) { return b.handle; });
  PointCloudRouting routing;
  if(has_points_input)
    routing = extractPointCloud(inputs.points.mesh);
  void* points_primary
      = has_points_input && !inputs.points.mesh.buffers.empty()
          ? inputs.points.mesh.buffers[0].handle
          : nullptr;
  int effective_count
      = routing.instance_count > 0 ? routing.instance_count
                                   : inputs.count.value;

  // Clamp instance_count to the capacity of the wired buffers. The user's
  // Count spinbox ranges up to 1000000 and is otherwise decoupled from the
  // Transforms buffer size; downstream ScenePreprocessor issues a strided
  // GPU copy of `instance_count` regions out of the source QRhiBuffer with
  // NO capacity guard, so an unclamped count reads far past the source
  // buffer -> Vulkan/RHI out-of-bounds copy / device-lost. We compute the
  // maximum instance count each source buffer can back, using the same
  // per-format stride as the preprocessor (translation=16, trs=40,
  // mat4=64 bytes), and clamp to the tightest constraint.
  {
    // Transform stride mirrors ScenePreprocessorNode::srcTranslationStride.
    uint32_t transform_stride = 64; // mat4
    if(routing.has_matrix)
      transform_stride = 64;
    else if(routing.transforms)
      transform_stride = 16; // translation
    else
    {
      switch(inputs.format.value)
      {
        case TRS:         transform_stride = 40; break;
        case Translation: transform_stride = 16; break;
        default:          transform_stride = 64; break;
      }
    }

    // Capacity (in instances) of a source buffer given a per-instance
    // stride. Returns -1 when there is no buffer to bound against.
    auto capacityFor
        = [](const ossia::buffer_resource_ptr& routed,
             const halp::gpu_buffer& raw, uint32_t stride) -> int64_t
    {
      if(stride == 0)
        return -1;
      int64_t byte_size = 0, byte_offset = 0;
      bool have = false;
      if(routed)
      {
        if(auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(&routed->resource))
        {
          if(gpu->native_handle)
          {
            byte_size = (int64_t)gpu->byte_size;
            byte_offset = (int64_t)gpu->byte_offset;
            have = true;
          }
        }
      }
      else if(raw.handle)
      {
        byte_size = raw.byte_size;
        byte_offset = raw.byte_offset;
        have = true;
      }
      if(!have)
        return -1;
      const int64_t avail = byte_size - byte_offset;
      return avail > 0 ? avail / (int64_t)stride : 0;
    };

    int64_t max_count = -1;
    const int64_t tcap
        = capacityFor(routing.transforms, inputs.transforms.buffer, transform_stride);
    if(tcap >= 0)
      max_count = tcap;
    // Colors are copied tightly at 16 bytes/instance (vec4).
    const int64_t ccap
        = capacityFor(routing.colors, inputs.colors.buffer, 16u);
    if(ccap >= 0)
      max_count = (max_count < 0) ? ccap : std::min(max_count, ccap);

    if(max_count >= 0 && (int64_t)effective_count > max_count)
      effective_count = (int)max_count;
  }

  // TRS recomputed; we reuse computeTRSMatrix from TransformHelper
  // even though we're not targeting a halp::mesh — the cache keeps the
  // update hooks simple.
  float scratch[16];
  CachedTRS xformCache = m_cachedTRS;
  computeTRSMatrix(inputs, scratch, xformCache);
  m_cachedTRS = xformCache;
  m_cached_in_state = in_state;
  m_cached_transforms = inputs.transforms.buffer.handle;
  m_cached_colors = inputs.colors.buffer.handle;
  m_cached_custom = inputs.custom.buffer.handle;
  m_cached_count = effective_count;
  m_cached_format = inputs.format.value;
  m_cached_points_buf = points_primary;
  m_cached_points_vertices = inputs.points.mesh.vertices;

  if(!proto)
  {
    // No prototype mesh → empty output (but leave the inputs wired,
    // so when a mesh appears later we pick it up on the next call).
    if(!m_wrapped_state)
      m_wrapped_state = std::make_shared<ossia::scene_state>();
    m_wrapped_state->roots.reset();
    m_wrapped_state->materials.reset();
    m_wrapped_state->version = ++m_version_counter;
    m_wrapped_state->dirty_index = m_version_counter;
    m_pending_dirty = 0xFF;
    return;
  }

  // Build the instance_component.
  // Transforms + colors: if a Points input is wired, prefer its
  // attributes (transform_matrix / translation / color0). Otherwise
  // fall back to the raw buffer inlets.
  auto inst = std::make_shared<ossia::instance_component>();
  inst->prototype = proto;
  inst->instance_count
      = effective_count > 0 ? uint32_t(effective_count) : 0u;
  inst->instance_transforms
      = routing.transforms
            ? routing.transforms
            : wrapGpuBuffer(inputs.transforms.buffer);
  inst->instance_colors
      = routing.colors
            ? routing.colors
            : wrapGpuBuffer(inputs.colors.buffer);
  inst->instance_custom = wrapGpuBuffer(inputs.custom.buffer);

  // Transform format: if the Points input provided a transform_matrix
  // attribute, force Mat4. Else if it provided translation/position,
  // force Translation. Else obey the user's combobox.
  if(routing.has_matrix)
  {
    inst->transform_type
        = ossia::instance_component::transform_format::mat4;
  }
  else if(routing.transforms)
  {
    inst->transform_type
        = ossia::instance_component::transform_format::translation;
  }
  else
  {
    switch(inputs.format.value)
    {
      case TRS:
        inst->transform_type = ossia::instance_component::transform_format::trs;
        break;
      case Translation:
        inst->transform_type
            = ossia::instance_component::transform_format::translation;
        break;
      default:
        inst->transform_type
            = ossia::instance_component::transform_format::mat4;
        break;
    }
  }
  inst->dirty_index = ++m_version_counter;

  // Wrap into a scene_node:
  //   child 0: local-controls scene_transform (Instancer's position /
  //            rotation / scale knobs). Updates parentWorld for every
  //            sibling that follows.
  //   child 1: prototype-ancestor scene_transform (the composed TRS
  //            that findFirstMesh accumulated walking down to the
  //            mesh upstream — e.g. the glTF root's scale, or a
  //            Primitive's TRS if it stamped one). Decomposed back
  //            into translation/quaternion/scale so the FlattenVisitor
  //            sees a normal scene_transform; the matrix is converted
  //            via Qt's decomposition on the off-chance the upstream
  //            TRS includes shear (rare). When the matrix is identity
  //            (no upstream transform), this is effectively a no-op
  //            but is always emitted to keep the child layout stable
  //            across rebuilds.
  //   child 2: the instance_component payload.
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

  // Decompose the prototype-ancestor TRS. QMatrix4x4 doesn't expose a
  // single TRS-decomposition call so we pull the columns: column 3 is
  // the translation; the upper-left 3×3's column lengths give scale;
  // the rotation matrix is the upper-left 3×3 with each column
  // normalised. Skips reconstruction (leaves identity defaults) when
  // protoWorld is the identity.
  ossia::scene_transform protoXform;
  protoXform.translation[0] = 0.f;
  protoXform.translation[1] = 0.f;
  protoXform.translation[2] = 0.f;
  protoXform.rotation[0] = 0.f;
  protoXform.rotation[1] = 0.f;
  protoXform.rotation[2] = 0.f;
  protoXform.rotation[3] = 1.f;
  protoXform.scale[0] = 1.f;
  protoXform.scale[1] = 1.f;
  protoXform.scale[2] = 1.f;
  if(!protoWorld.isIdentity())
  {
    const float* d = protoWorld.constData();
    protoXform.translation[0] = d[12];
    protoXform.translation[1] = d[13];
    protoXform.translation[2] = d[14];
    QVector3D c0(d[0], d[1], d[2]);
    QVector3D c1(d[4], d[5], d[6]);
    QVector3D c2(d[8], d[9], d[10]);
    protoXform.scale[0] = c0.length();
    protoXform.scale[1] = c1.length();
    protoXform.scale[2] = c2.length();
    if(protoXform.scale[0] > 1e-6f) c0 /= protoXform.scale[0];
    if(protoXform.scale[1] > 1e-6f) c1 /= protoXform.scale[1];
    if(protoXform.scale[2] > 1e-6f) c2 /= protoXform.scale[2];
    QMatrix3x3 rotmat;
    rotmat(0,0)=c0.x(); rotmat(1,0)=c0.y(); rotmat(2,0)=c0.z();
    rotmat(0,1)=c1.x(); rotmat(1,1)=c1.y(); rotmat(2,1)=c1.z();
    rotmat(0,2)=c2.x(); rotmat(1,2)=c2.y(); rotmat(2,2)=c2.z();
    QQuaternion pq = QQuaternion::fromRotationMatrix(rotmat);
    protoXform.rotation[0] = pq.x();
    protoXform.rotation[1] = pq.y();
    protoXform.rotation[2] = pq.z();
    protoXform.rotation[3] = pq.scalar();
  }
  // raw_slot stays default (invalid) — this is a synthesized child and
  // doesn't need a registry slot. The FlattenVisitor's scene_transform
  // branch composes regardless of slot validity.

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(xform);
  children->push_back(protoXform);
  children->push_back(ossia::instance_component_ptr(std::move(inst)));

  auto node = std::make_shared<ossia::scene_node>();
  node->children = std::move(children);
  node->dirty_index = m_version_counter;

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(node));

  if(!m_wrapped_state)
    m_wrapped_state = std::make_shared<ossia::scene_state>();
  m_wrapped_state->roots = std::move(roots);
  // Pass through materials / animations / cameras / env from the
  // input so PBR shaders still have their material table.
  if(in.state)
  {
    m_wrapped_state->materials = in.state->materials;
    m_wrapped_state->animations = in.state->animations;
    m_wrapped_state->cameras = in.state->cameras;
    m_wrapped_state->skeletons = in.state->skeletons;
    m_wrapped_state->environment = in.state->environment;
    m_wrapped_state->active_camera_id = in.state->active_camera_id;
  }
  m_wrapped_state->version = m_version_counter;
  m_wrapped_state->dirty_index = m_version_counter;
  m_pending_dirty = 0xFF;
}

void Instancer::operator()()
{
  // Upstream scene_state / buffer-handle / point-cloud dirty flags can
  // change without a port-update event — detect here and call
  // rebuild(). Controls themselves trigger rebuild via update().
  //
  // The Points-input cache also has to compare the current vertex count
  // and the primary buffer handle against the cached values written in
  // rebuild() (m_cached_points_vertices / m_cached_points_buf). When an
  // upstream CSF compute regenerates its point cloud with a different
  // count (3500 → 4000) but reuses the same persistent QRhiBuffer, the
  // dirty_mesh flag is NOT set (the buffer handle didn't change), and
  // without these comparisons Instancer kept publishing the stale
  // instance_count. Downstream ScenePreprocessor's update() then took
  // its meshesUnchanged early-return; the persistent m_pendingGpuCopies
  // queue kept firing the OLD count for the GPU translation/color copy,
  // appearing as "instances frozen at the previous count, then snapping
  // back at random intervals" whenever some unrelated rebuild kicked in.
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  void* points_primary
      = !inputs.points.mesh.buffers.empty()
            ? inputs.points.mesh.buffers[0].handle
            : nullptr;
  const bool upstream_changed
      = m_cached_in_state != in_state
        || m_cached_transforms != inputs.transforms.buffer.handle
        || m_cached_colors != inputs.colors.buffer.handle
        || m_cached_custom != inputs.custom.buffer.handle
        || m_cached_points_buf != points_primary
        || m_cached_points_vertices != inputs.points.mesh.vertices
        || inputs.points.dirty_mesh;
  if(!m_wrapped_state || upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_wrapped_state;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

void Instancer::init(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
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

void Instancer::update(
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

void Instancer::release(score::gfx::RenderList& r)
{
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_xform_ref = {};
  // Producer-state-drift Option A — see Light::release.
  m_wrapped_state.reset();
}

} // namespace Threedim

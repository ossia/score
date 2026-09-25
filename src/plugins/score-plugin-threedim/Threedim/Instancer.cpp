#include "Instancer.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QDebug>
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

// Extract the first mesh_component in a scene tree, depth-first, along with the
// scene_transform composition accumulated on the path to it. That composition
// is how upstream producers position their meshes -- a glTF root's scale, a
// Primitive's TRS -- so instancing without it would draw at the model's
// intrinsic origin and scale.
//
// Two deliberate behaviours: only the first mesh in depth-first order is
// instanced, and sibling scene_transforms BEFORE the mesh are composed, per the
// FlattenVisitor's "transform applies to subsequent siblings" contract, while
// ones after it are not.
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
  // The transforms attribute's ACTUAL per-instance stride, taken from the
  // binding it resolves through. Assuming 16 (a vec4 translation) misreads a
  // tightly packed float3 cloud, whose stride is 12: a 24-byte buffer then
  // looks like ONE instance instead of two, and the cloud silently loses
  // points. 0 = unknown, fall back to the per-format default.
  uint32_t transform_stride{0};
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
        if(attr.binding >= 0 && attr.binding < (int)mesh.bindings.size())
          out.transform_stride = (uint32_t)mesh.bindings[attr.binding].stride;
        out.has_matrix = true;
        break;
      case S::translation:
      case S::position:
        if(!out.has_matrix && !out.transforms)
          out.transforms = wrapAttributeAsBuffer(mesh, attr);
        if(attr.binding >= 0 && attr.binding < (int)mesh.bindings.size())
          out.transform_stride = (uint32_t)mesh.bindings[attr.binding].stride;
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

// Fold every Points buffer handle into one fingerprint. rebuild() routes
// instance_transforms / instance_colors from arbitrary attribute buffers and
// stores those raw QRhiBuffer* in the persistent m_wrapped_state. Caching only
// buffers[0] would leave a dangling handle when a producer reallocates a
// secondary buffer while keeping buffers[0], the vertex count and dirty_mesh
// unchanged.
uintptr_t pointsBufferFingerprint(
    const halp::dynamic_gpu_geometry& mesh) noexcept
{
  uintptr_t fp = 1469598103934665603ull; // FNV-1a offset basis
  for(const auto& b : mesh.buffers)
  {
    // Fold the SIZE too, not just the handle: a producer can shrink a buffer
    // in place and keep the pointer. (geometry_gpu_buffer carries no offset.)
    fp ^= reinterpret_cast<uintptr_t>(b.handle);
    fp *= 1099511628211ull;
    fp ^= uintptr_t(b.byte_size);
    fp *= 1099511628211ull;
  }
  return fp;
}

const QString placementShader = QStringLiteral(R"_(#version 450
layout(local_size_x = 64) in;

layout(std140, binding = 0) uniform Params
{
  mat4 inverse_linear;
  uvec4 source_layout;
};

layout(std430, binding = 1) readonly buffer Source
{
  float source_data[];
};

layout(std430, binding = 2) writeonly buffer Target
{
  vec4 target_data[];
};

void main()
{
  uint i = gl_GlobalInvocationID.x;
  if(i >= source_layout.x)
    return;
  uint b = source_layout.z + i * source_layout.y;
  vec3 t = vec3(source_data[b], source_data[b + 1u], source_data[b + 2u]);
  float w = source_layout.w != 0u ? source_data[b + 3u] : 1.0;
  target_data[i] = vec4((inverse_linear * vec4(t, 0.0)).xyz, w);
}
)_");

struct PlacementParams
{
  float inverse_linear[16];
  uint32_t count;
  uint32_t stride;
  uint32_t offset;
  uint32_t has_w;
};

} // namespace

bool Instancer::preparePlacement(
    const ossia::buffer_resource_ptr& routed, const halp::gpu_buffer& raw,
    uint32_t stride, uint32_t column_offset, bool has_w, uint32_t count,
    const float* inverse_linear)
{
  m_placing = false;
  if(!m_rhi || !m_placePipeline || count == 0 || stride % 4 != 0)
    return false;

  QRhiBuffer* source{};
  int64_t offset{};
  if(routed)
  {
    if(auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(&routed->resource))
    {
      source = static_cast<QRhiBuffer*>(gpu->native_handle);
      offset = (int64_t)gpu->byte_offset;
    }
  }
  else
  {
    source = static_cast<QRhiBuffer*>(raw.handle);
    offset = raw.byte_offset;
  }
  offset += column_offset;
  if(!source || offset % 4 != 0 || !(source->usage() & QRhiBuffer::StorageBuffer))
    return false;

  const quint32 bytes = count * 16;
  if(!m_placed)
  {
    m_placed = m_rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::UsageFlags(score::gfx::compatibleBufferUsage(
            *m_rhi, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer)),
        bytes);
    m_placed->setName("Instancer::placed_translations");
    if(!m_placed->create())
    {
      delete m_placed;
      m_placed = nullptr;
      return false;
    }
    m_placeSrbDirty = true;
  }
  else if(m_placed->size() < bytes)
  {
    m_placed->destroy();
    m_placed->setSize(bytes);
    if(!m_placed->create())
      return false;
    m_placeSrbDirty = true;
  }

  if(m_placement.source != source)
    m_placeSrbDirty = true;
  m_placement.source = source;
  m_placement.source_offset = (uint32_t)offset;
  m_placement.source_stride = stride;
  m_placement.count = count;
  m_placement.has_w = has_w ? 1u : 0u;
  std::copy_n(inverse_linear, 16, m_placement.inverse_linear);
  m_placing = true;
  return true;
}

void Instancer::rebuild()
{
  m_placing = false;
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

  // Point-cloud input takes precedence over the raw buffer inlets when it is
  // wired, that is, when at least one buffer in the points mesh has a non-null
  // handle. The routing struct populates transforms / colors from the matching
  // attribute semantics; empty routing falls back to the raw buffer ports.
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

  // Clamp instance_count to the capacity of the wired buffers. The Count spinbox
  // goes to 1000000 and is otherwise decoupled from the Transforms buffer size,
  // while the downstream ScenePreprocessor issues a strided GPU copy of
  // instance_count regions with no capacity guard. Compute the maximum each
  // source buffer can back, using the preprocessor's per-format strides
  // (translation 16, trs 40, mat4 64 bytes), and take the tightest.
  {
    // Transform stride mirrors ScenePreprocessorNode::srcTranslationStride.
    uint32_t transform_stride = 64; // mat4
    if(routing.has_matrix)
      transform_stride = 64;
    else if(routing.transforms)
      transform_stride
          = routing.transform_stride > 0 ? routing.transform_stride : 16u;
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

  // computeTRSMatrix from TransformHelper is reused here even though the
  // target is not a halp::mesh — the cache keeps the update hooks simple.
  float scratch[16];
  CachedTRS xformCache = m_cachedTRS;
  computeTRSMatrix(inputs, scratch, xformCache);
  m_cachedTRS = xformCache;
  m_cached_in_state = in_state;
  m_cached_transforms = viewOf(inputs.transforms.buffer);
  m_cached_colors = viewOf(inputs.colors.buffer);
  m_cached_custom = viewOf(inputs.custom.buffer);
  m_cached_count = effective_count;
  m_cached_format = inputs.format.value;
  m_cached_points_buf = points_primary;
  m_cached_points_vertices = inputs.points.mesh.vertices;
  m_cached_points_fingerprint = pointsBufferFingerprint(inputs.points.mesh);

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
  {
    using TF = ossia::instance_component::transform_format;
    uint32_t stride = 16, column = 0;
    bool has_w = false;
    switch(inst->transform_type)
    {
      case TF::mat4:
        stride = 64;
        column = 48;
        break;
      case TF::trs:
        stride = 40;
        break;
      case TF::translation:
        stride = routing.transforms && routing.transform_stride > 0
                     ? routing.transform_stride
                     : 16u;
        has_w = stride >= 16;
        break;
    }

    QMatrix4x4 linear = protoWorld;
    linear.setColumn(3, QVector4D{0.f, 0.f, 0.f, 1.f});
    bool invertible = false;
    const QMatrix4x4 inverse = linear.inverted(&invertible);

    const bool placed
        = invertible
          && preparePlacement(
              routing.transforms, inputs.transforms.buffer, stride, column, has_w,
              inst->instance_count, inverse.constData());
    if(!placed && m_rhi && !m_placePipeline && inst->instance_count > 0
       && !linear.isIdentity() && !m_warnedPlacementFallback)
    {
      m_warnedPlacementFallback = true;
      qWarning() << "Instancer: no compute support on this backend, instance "
                    "translations are scaled by the prototype transform";
    }
    if(placed)
    {
      ossia::gpu_buffer_handle gh;
      gh.native_handle = m_placed;
      gh.byte_size = int64_t(inst->instance_count) * 16;
      gh.byte_offset = 0;
      auto placed = std::make_shared<ossia::buffer_resource>();
      placed->resource = gh;
      placed->dirty_index = 1;
      inst->instance_transforms = std::move(placed);
      inst->transform_type = TF::translation;
    }
  }
  inst->dirty_index = ++m_version_counter;

  // Wrap into a scene_node:
  //   child 0: local-controls scene_transform (this Instancer's position,
  //            rotation and scale knobs), updating parentWorld for the siblings
  //            that follow.
  //   child 1: prototype-ancestor scene_transform, the composed TRS findFirstMesh
  //            accumulated walking down to the mesh, decomposed back into
  //            translation / quaternion / scale through Qt so the FlattenVisitor
  //            sees a normal scene_transform. Always emitted, even when identity,
  //            to keep the child layout stable across rebuilds.
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

  // Decompose the prototype-ancestor TRS. QMatrix4x4 exposes no single
  // TRS-decomposition call, so the columns are taken apart here: column 3 is
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
    // A length is never negative, so a REFLECTION -- any TRS with an odd
    // number of negative scale axes, determinant < 0 -- comes out of the above
    // as all-positive scale, and the basis would be improper (det -1), which is
    // not a rotation and has no quaternion. Uncorrected, the reflection is
    // silently dropped: a prototype scaled by -1 in x maps +1 to +1.
    //
    // Fold the sign back onto one axis. Which axis is arbitrary -- any single
    // negated axis reproduces the same linear map once the rotation is taken
    // from the corrected basis -- so use x by convention, and correct the basis
    // BEFORE building the rotation so it is proper.
    if(QVector3D::dotProduct(c0, QVector3D::crossProduct(c1, c2)) < 0.f)
    {
      protoXform.scale[0] = -protoXform.scale[0];
      c0 = -c0;
    }
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

bool Instancer::refresh()
{
  // Upstream scene_state, buffer-handle and point-cloud dirty flags can change
  // without a port-update event, so detect them here and call rebuild(); controls
  // go through update().
  //
  // A producer may change vertex count while keeping the same QRhiBuffer and
  // without raising dirty_mesh.
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  void* points_primary
      = !inputs.points.mesh.buffers.empty()
            ? inputs.points.mesh.buffers[0].handle
            : nullptr;
  const bool upstream_changed
      = m_cached_in_state != in_state
        || m_cached_transforms != viewOf(inputs.transforms.buffer)
        || m_cached_colors != viewOf(inputs.colors.buffer)
        || m_cached_custom != viewOf(inputs.custom.buffer)
        || m_cached_points_buf != points_primary
        || m_cached_points_vertices != inputs.points.mesh.vertices
        || m_cached_points_fingerprint
               != pointsBufferFingerprint(inputs.points.mesh)
        || inputs.points.dirty_mesh;
  if(m_wrapped_state && !upstream_changed)
    return false;
  rebuild();
  return true;
}

void Instancer::operator()()
{
  refresh();
  outputs.scene_out.scene.state = m_wrapped_state;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

void Instancer::init(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  m_rhi = r.state.rhi;
  if(m_rhi && m_rhi->isFeatureSupported(QRhi::Compute) && !m_placePipeline)
  {
    QShader shader = score::gfx::makeCompute(r.state, placementShader);
    if(shader.isValid())
    {
      m_placeParams = m_rhi->newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(PlacementParams));
      m_placeParams->setName("Instancer::placement_params");
      m_placeSrb = m_rhi->newShaderResourceBindings();
      m_placePipeline = m_rhi->newComputePipeline();
      m_placePipeline->setShaderStage({QRhiShaderStage::Compute, shader});
      if(!m_placeParams->create())
      {
        delete m_placeParams;
        delete m_placeSrb;
        delete m_placePipeline;
        m_placeParams = nullptr;
        m_placeSrb = nullptr;
        m_placePipeline = nullptr;
      }
    }
    m_placeSrbDirty = true;
  }
  m_wrapped_state.reset();

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

void Instancer::runInitialPasses(
    score::gfx::RenderList& r, QRhiCommandBuffer& cb,
    QRhiResourceUpdateBatch*& res, score::gfx::Edge&)
{
  refresh();
  if(!m_placing || !m_placePipeline || !m_placed || !res)
    return;

  if(m_placeSrbDirty)
  {
    m_placeSrb->destroy();
    m_placeSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::ComputeStage, m_placeParams),
        QRhiShaderResourceBinding::bufferLoad(
            1, QRhiShaderResourceBinding::ComputeStage, m_placement.source),
        QRhiShaderResourceBinding::bufferStore(
            2, QRhiShaderResourceBinding::ComputeStage, m_placed),
    });
    if(!m_placeSrb->create())
      return;
    if(!m_placePipeline->shaderResourceBindings())
    {
      m_placePipeline->setShaderResourceBindings(m_placeSrb);
      if(!m_placePipeline->create())
      {
        m_placePipeline->setShaderResourceBindings(nullptr);
        return;
      }
    }
    m_placeSrbDirty = false;
  }

  PlacementParams params{};
  std::copy_n(m_placement.inverse_linear, 16, params.inverse_linear);
  params.count = m_placement.count;
  params.stride = m_placement.source_stride / 4;
  params.offset = m_placement.source_offset / 4;
  params.has_w = m_placement.has_w;
  res->updateDynamicBuffer(m_placeParams, 0, sizeof(params), &params);

  cb.beginComputePass(res);
  cb.setComputePipeline(m_placePipeline);
  cb.setShaderResources(m_placeSrb);
  cb.dispatch((m_placement.count + 63) / 64, 1, 1);
  cb.endComputePass();
  res = r.state.rhi->nextResourceUpdateBatch();
}

void Instancer::release(score::gfx::RenderList& r)
{
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_xform_ref = {};
  m_wrapped_state.reset();

  delete m_placePipeline;
  delete m_placeSrb;
  delete m_placeParams;
  delete m_placed;
  m_placePipeline = nullptr;
  m_placeSrb = nullptr;
  m_placeParams = nullptr;
  m_placed = nullptr;
  m_placement = {};
  m_placing = false;
  m_placeSrbDirty = true;
  m_rhi = nullptr;
}

} // namespace Threedim

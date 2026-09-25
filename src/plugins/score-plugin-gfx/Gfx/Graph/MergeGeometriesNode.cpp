#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <tuple>

namespace score::gfx
{

namespace
{
bool isIdentity(const ossia::transform3d& t) noexcept
{
  static constexpr ossia::transform3d identity{};
  return std::equal(std::begin(t.matrix), std::end(t.matrix), std::begin(identity.matrix));
}

void transformPoint(const float* m, float* p) noexcept
{
  const float x = p[0], y = p[1], z = p[2];
  p[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
  p[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
  p[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
}

void transformDirection(const float* m3, float* d) noexcept
{
  const float x = d[0], y = d[1], z = d[2];
  float r[3] = {
      m3[0] * x + m3[3] * y + m3[6] * z, m3[1] * x + m3[4] * y + m3[7] * z,
      m3[2] * x + m3[5] * y + m3[8] * z};
  const float len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
  if(len > 0.f)
  {
    d[0] = r[0] / len;
    d[1] = r[1] / len;
    d[2] = r[2] / len;
  }
}

struct BakeMatrices
{
  const float* model{};
  float linear[9]{};
  float normal[9]{};

  explicit BakeMatrices(const ossia::transform3d& t)
      : model{t.matrix}
  {
    const float* m = t.matrix;
    for(int c = 0; c < 3; c++)
      for(int r = 0; r < 3; r++)
        linear[c * 3 + r] = m[c * 4 + r];

    const float a = m[0], b = m[4], c = m[8];
    const float d = m[1], e = m[5], f = m[9];
    const float g = m[2], h = m[6], i = m[10];
    const float cof[9] = {
        e * i - f * h, -(d * i - f * g), d * h - e * g,
        -(b * i - c * h), a * i - c * g, -(a * h - b * g),
        b * f - c * e, -(a * f - c * d), a * e - b * d};
    const float det = a * cof[0] + b * cof[1] + c * cof[2];
    const float sign = det < 0.f ? -1.f : 1.f;
    // Inverse-transpose of the upper 3x3, up to a positive scale, column-major.
    normal[0] = sign * cof[0];
    normal[1] = sign * cof[3];
    normal[2] = sign * cof[6];
    normal[3] = sign * cof[1];
    normal[4] = sign * cof[4];
    normal[5] = sign * cof[7];
    normal[6] = sign * cof[2];
    normal[7] = sign * cof[5];
    normal[8] = sign * cof[8];
  }
};

int floatComponents(decltype(ossia::geometry::attribute::format) f) noexcept
{
  switch(f)
  {
    case ossia::geometry::attribute::float3:
      return 3;
    case ossia::geometry::attribute::float4:
      return 4;
    default:
      return 0;
  }
}

const char* formatName(decltype(ossia::geometry::attribute::format) f) noexcept
{
  using A = ossia::geometry::attribute;
  switch(f)
  {
    case A::float4: return "float4";
    case A::float3: return "float3";
    case A::float2: return "float2";
    case A::float1: return "float1";
    case A::unormbyte4: return "unormbyte4";
    case A::unormbyte2: return "unormbyte2";
    case A::unormbyte1: return "unormbyte1";
    case A::uint4: return "uint4";
    case A::uint3: return "uint3";
    case A::uint2: return "uint2";
    case A::uint1: return "uint1";
    case A::sint4: return "sint4";
    case A::sint3: return "sint3";
    case A::sint2: return "sint2";
    case A::sint1: return "sint1";
    case A::half4: return "half4";
    case A::half3: return "half3";
    case A::half2: return "half2";
    case A::half1: return "half1";
    case A::ushort4: return "ushort4";
    case A::ushort3: return "ushort3";
    case A::ushort2: return "ushort2";
    case A::ushort1: return "ushort1";
    case A::sshort4: return "sshort4";
    case A::sshort3: return "sshort3";
    case A::sshort2: return "sshort2";
    case A::sshort1: return "sshort1";
    case A::user_struct: return "user_struct";
  }
  return "unknown";
}

using UntransformedReport
    = std::function<void(const ossia::geometry::attribute&, const char* reason)>;

void transformBounds(ossia::geometry& g, const float* m) noexcept
{
  const auto& b = g.bounds;
  if(b.min[0] > b.max[0] || b.min[1] > b.max[1] || b.min[2] > b.max[2])
    return;
  if(std::equal(std::begin(b.min), std::end(b.min), std::begin(b.max)))
    return;
  float lo[3], hi[3];
  for(int k = 0; k < 8; k++)
  {
    float p[3]
        = {(k & 1) ? b.max[0] : b.min[0], (k & 2) ? b.max[1] : b.min[1],
           (k & 4) ? b.max[2] : b.min[2]};
    transformPoint(m, p);
    for(int a = 0; a < 3; a++)
    {
      lo[a] = k == 0 ? p[a] : std::min(lo[a], p[a]);
      hi[a] = k == 0 ? p[a] : std::max(hi[a], p[a]);
    }
  }
  std::copy_n(lo, 3, g.bounds.min);
  std::copy_n(hi, 3, g.bounds.max);
}

int bakeKind(ossia::attribute_semantic s) noexcept
{
  switch(s)
  {
    case ossia::attribute_semantic::position:
      return 0;
    case ossia::attribute_semantic::normal:
      return 1;
    case ossia::attribute_semantic::tangent:
    case ossia::attribute_semantic::bitangent:
      return 2;
    default:
      return -1;
  }
}

constexpr int kMaxGpuBakeAttributes = 8;
constexpr uint32_t kGpuBakeLocalSize = 256;
constexpr uint32_t kGpuBakeMaxGroupsX = 65535;

struct GpuBakeParams
{
  float model[16];
  float normal[12];
  float linear[12];
  uint32_t counts[4];
  uint32_t attributes[kMaxGpuBakeAttributes][4];
};
static_assert(sizeof(GpuBakeParams) == 64 + 48 + 48 + 16 + 16 * kMaxGpuBakeAttributes);

const QString& gpuBakeShader()
{
  static const QString code = QStringLiteral(R"(#version 450
layout(local_size_x = 256) in;

layout(std140, binding = 0) uniform Params {
  mat4 model;
  mat3 normalMatrix;
  mat3 linearMatrix;
  uvec4 counts;
  uvec4 attributes[8];
};

layout(std430, binding = 1) readonly buffer Source { uint src[]; };
layout(std430, binding = 2) writeonly buffer Destination { uint dst[]; };

void main()
{
  uint w = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * counts.z;
  if(w >= counts.x)
    return;

  uint value = src[w];
  for(uint a = 0u; a < counts.y; a++)
  {
    uvec4 attr = attributes[a];
    if(w < attr.x)
      continue;
    uint rel = w - attr.x;
    uint v = rel / attr.y;
    uint c = rel % attr.y;
    if(v >= attr.z || c >= 3u)
      continue;

    uint base = attr.x + v * attr.y;
    vec3 p = vec3(
        uintBitsToFloat(src[base]), uintBitsToFloat(src[base + 1u]),
        uintBitsToFloat(src[base + 2u]));
    vec3 r;
    if(attr.w == 0u)
    {
      r = (model * vec4(p, 1.0)).xyz;
    }
    else
    {
      r = (attr.w == 1u ? normalMatrix : linearMatrix) * p;
      float len = length(r);
      r = len > 0.0 ? r / len : p;
    }
    value = floatBitsToUint(r[c]);
    break;
  }
  dst[w] = value;
}
)");
  return code;
}
}

namespace
{
std::vector<ossia::geometry> bakeCpuAttributes(
    const std::vector<ossia::geometry>& meshes, const ossia::transform3d& transform,
    const UntransformedReport& report)
{
  std::vector<ossia::geometry> out = meshes;
  if(isIdentity(transform))
    return out;

  const BakeMatrices mats{transform};
  std::map<const void*, std::shared_ptr<void>> copies;
  std::set<std::tuple<const void*, int64_t, int>> done;

  for(auto& g : out)
  {
    for(const auto& attr : g.attributes)
    {
      const int kind = bakeKind(attr.semantic);
      if(kind < 0)
        continue;
      if(attr.binding < 0 || attr.binding >= std::ssize(g.input)
         || attr.binding >= std::ssize(g.bindings))
        continue;
      const auto& in = g.input[attr.binding];
      if(in.buffer < 0 || in.buffer >= std::ssize(g.buffers))
        continue;
      auto& buf = g.buffers[in.buffer];
      auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&buf.data);
      if(!cpu || !cpu->raw_data || cpu->byte_size <= 0)
        continue;
      const int comps = floatComponents(attr.format);
      if(comps == 0)
      {
        if(report)
          report(attr, "only float3 and float4 are transformed");
        continue;
      }
      if(g.bindings[attr.binding].classification
         != ossia::geometry::binding::per_vertex)
      {
        if(report)
          report(attr, "per-instance attributes are not transformed");
        continue;
      }

      const void* original = cpu->raw_data.get();
      const int64_t start = in.byte_offset + attr.byte_offset;
      if(!done.emplace(original, start, kind).second)
        continue;

      auto& copy = copies[original];
      if(!copy)
      {
        auto* bytes = new unsigned char[cpu->byte_size];
        std::memcpy(bytes, original, cpu->byte_size);
        copy = std::shared_ptr<void>(bytes, [](void* p) { delete[] (unsigned char*)p; });
      }

      const int64_t stride = g.bindings[attr.binding].byte_stride > 0
                                 ? g.bindings[attr.binding].byte_stride
                                 : int64_t(comps * sizeof(float));
      auto* base = static_cast<unsigned char*>(copy.get());
      for(int64_t v = 0; v < g.vertices; v++)
      {
        const int64_t off = start + v * stride;
        if(off < 0 || off + 3 * int64_t(sizeof(float)) > cpu->byte_size)
          break;
        float p[3];
        std::memcpy(p, base + off, sizeof(p));
        if(kind == 0)
          transformPoint(mats.model, p);
        else if(kind == 1)
          transformDirection(mats.normal, p);
        else
          transformDirection(mats.linear, p);
        std::memcpy(base + off, p, sizeof(p));
      }
    }

    for(auto& buf : g.buffers)
    {
      if(auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&buf.data))
      {
        if(auto it = copies.find(cpu->raw_data.get()); it != copies.end())
        {
          cpu->raw_data = it->second;
          buf.dirty = true;
        }
      }
    }
    transformBounds(g, mats.model);
  }
  return out;
}
}

std::vector<ossia::geometry> bakeGeometryTransform(
    const std::vector<ossia::geometry>& meshes, const ossia::transform3d& transform)
{
  return bakeCpuAttributes(meshes, transform, {});
}


struct RenderedMergeGeometriesNode final : NodeRenderer
{
  const MergeGeometriesNode& m_node;
  ossia::geometry_spec m_outputSpec;
  std::array<ossia::geometry_spec, MergeGeometriesNode::kMaxInputs> m_cachedInputs;
  std::array<ossia::transform3d, MergeGeometriesNode::kMaxInputs> m_transforms;
  std::array<int64_t, MergeGeometriesNode::kMaxInputs> m_cachedDirtyIndex{};
  bool m_transformsChanged{};

  struct GpuBake
  {
    QRhiBuffer* output{};
    QRhiBuffer* ubo{};
    QRhiShaderResourceBindings* srb{};
    QRhiBuffer* boundSource{};
    QRhiBuffer* boundOutput{};
    int64_t words{};
    GpuBakeParams params{};
    int attributeCount{};
    bool used{};
  };
  std::map<std::pair<int, QRhiBuffer*>, GpuBake> m_gpuBakes;
  QRhiComputePipeline* m_gpuBakePipeline{};
  bool m_gpuBakeUnavailable{};
  int64_t m_gpuBakeFrame{-1};
  std::set<std::tuple<int, int, std::string>> m_warnedUntransformed;

  RenderedMergeGeometriesNode(const MergeGeometriesNode& n)
      : NodeRenderer{n}
      , m_node{n}
  {
  }

  void init(RenderList&, QRhiResourceUpdateBatch&) override { m_initialized = true; }

  void releaseGpuBake(RenderList& renderer, GpuBake& bake)
  {
    renderer.releaseBuffer(bake.output);
    if(bake.ubo)
      bake.ubo->deleteLater();
    if(bake.srb)
      bake.srb->deleteLater();
    bake = {};
  }

  void release(RenderList& renderer) override
  {
    for(auto& [key, bake] : m_gpuBakes)
      releaseGpuBake(renderer, bake);
    m_gpuBakes.clear();
    if(m_gpuBakePipeline)
      m_gpuBakePipeline->deleteLater();
    m_gpuBakePipeline = nullptr;
    m_gpuBakeUnavailable = false;
    m_gpuBakeFrame = -1;
    m_outputSpec = {};
    for(auto& c : m_cachedInputs)
      c = {};
    m_cachedDirtyIndex = {};
    m_initialized = false;
  }

  void process(int32_t port, const ossia::transform3d& v) override
  {
    if(port < 0 || port >= MergeGeometriesNode::kMaxInputs)
      return;
    if(!std::equal(
           std::begin(v.matrix), std::end(v.matrix),
           std::begin(m_transforms[port].matrix)))
    {
      m_transforms[port] = v;
      m_transformsChanged = true;
    }
  }

  // m_portGeometries is keyed by (port, source), so look up the first entry
  // matching the requested port. MergeGeometriesNode wires one input
  // per port, so multi-source convergence on a single port isn't expected
  // here; take the first match.
  const ossia::geometry_spec* findFirstByPort(int32_t port) const
  {
    for(const auto& [k, v] : m_portGeometries)
      if(k.first == port)
        return &v;
    return nullptr;
  }

  bool anyInputChanged() const
  {
    for(int i = 0; i < MergeGeometriesNode::kMaxInputs; ++i)
    {
      const auto* found = findFirstByPort((int32_t)i);
      const ossia::geometry_spec& cur
          = found ? *found : ossia::geometry_spec{};
      if(!(cur == m_cachedInputs[i]))
        return true;
      if(cur.meshes && !isIdentity(m_transforms[i]))
      {
        if(cur.meshes->dirty_index != m_cachedDirtyIndex[i])
          return true;
        for(const auto& g : cur.meshes->meshes)
          for(const auto& b : g.buffers)
            if(b.dirty)
              return true;
      }
    }
    return false;
  }

  bool ensureGpuBake(RenderList& renderer, QRhiBuffer* source, GpuBake& bake)
  {
    auto& rhi = *renderer.state.rhi;
    const int64_t bytes = bake.words * 4;
    if(!bake.output || bake.output->size() != bytes)
    {
      renderer.releaseBuffer(bake.output);
      bake.output = nullptr;
      QRhiBuffer::UsageFlags usage = source->usage();
      usage.setFlag(QRhiBuffer::UniformBuffer, false);
      usage |= QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer;
      auto* out = rhi.newBuffer(QRhiBuffer::Static, usage, bytes);
      out->setName("MergeGeometriesNode::gpuBake");
      if(!out->create())
      {
        delete out;
        return false;
      }
      RenderList::noteBufferLive(out);
      bake.output = out;
    }

    if(!bake.ubo)
    {
      bake.ubo = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(GpuBakeParams));
      bake.ubo->setName("MergeGeometriesNode::gpuBakeUBO");
      if(!bake.ubo->create())
        return false;
    }

    if(!bake.srb || bake.boundSource != source || bake.boundOutput != bake.output)
    {
      if(!bake.srb)
        bake.srb = rhi.newShaderResourceBindings();
      bake.srb->setBindings({
          QRhiShaderResourceBinding::uniformBuffer(
              0, QRhiShaderResourceBinding::ComputeStage, bake.ubo),
          QRhiShaderResourceBinding::bufferLoad(
              1, QRhiShaderResourceBinding::ComputeStage, source),
          QRhiShaderResourceBinding::bufferStore(
              2, QRhiShaderResourceBinding::ComputeStage, bake.output),
      });
      bake.boundSource = nullptr;
      if(!bake.srb->create())
        return false;
      bake.boundSource = source;
      bake.boundOutput = bake.output;
    }

    if(!m_gpuBakePipeline)
    {
      QShader shader;
      try
      {
        shader = score::gfx::makeCompute(renderer.state, gpuBakeShader());
      }
      catch(...)
      {
        m_gpuBakeUnavailable = true;
        return false;
      }
      m_gpuBakePipeline = rhi.newComputePipeline();
      m_gpuBakePipeline->setShaderStage({QRhiShaderStage::Compute, shader});
      m_gpuBakePipeline->setShaderResourceBindings(bake.srb);
      if(!m_gpuBakePipeline->create())
      {
        delete m_gpuBakePipeline;
        m_gpuBakePipeline = nullptr;
        m_gpuBakeUnavailable = true;
        return false;
      }
    }
    return true;
  }

  void warnUntransformed(const ossia::geometry::attribute& attr, const char* reason)
  {
    if(!m_warnedUntransformed.emplace(int(attr.semantic), int(attr.format), reason)
            .second)
      return;
    const auto name = ossia::semantic_to_name(attr.semantic);
    qWarning().noquote() << "Merge Geometries:"
                         << QString::fromUtf8(name.data(), qsizetype(name.size()))
                         << "attribute in" << formatName(attr.format)
                         << "passes untransformed:" << reason;
  }

  void bakeGpuBuffers(
      RenderList& renderer, int port, std::vector<ossia::geometry>& meshes,
      const ossia::transform3d& transform)
  {
    const BakeMatrices mats{transform};
    std::set<std::tuple<QRhiBuffer*, int64_t, int>> done;
    std::vector<QRhiBuffer*> sources;

    for(auto& g : meshes)
    {
      for(const auto& attr : g.attributes)
      {
        const int kind = bakeKind(attr.semantic);
        if(kind < 0)
          continue;
        if(attr.binding < 0 || attr.binding >= std::ssize(g.input)
           || attr.binding >= std::ssize(g.bindings))
          continue;
        const auto& in = g.input[attr.binding];
        if(in.buffer < 0 || in.buffer >= std::ssize(g.buffers))
          continue;
        auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&g.buffers[in.buffer].data);
        if(!gpu || !gpu->handle || gpu->byte_size <= 0)
          continue;
        const int comps = floatComponents(attr.format);
        if(comps == 0)
        {
          warnUntransformed(attr, "only float3 and float4 are transformed");
          continue;
        }
        if(g.bindings[attr.binding].classification
           != ossia::geometry::binding::per_vertex)
        {
          warnUntransformed(attr, "per-instance attributes are not transformed");
          continue;
        }
        if(!m_gpuBakeUnavailable
           && (!renderer.state.rhi
               || !renderer.state.rhi->isFeatureSupported(QRhi::Compute)))
          m_gpuBakeUnavailable = true;
        if(m_gpuBakeUnavailable)
        {
          warnUntransformed(attr, "GPU buffers need compute shaders");
          continue;
        }
        auto* source = static_cast<QRhiBuffer*>(gpu->handle);
        if(!source->usage().testFlag(QRhiBuffer::StorageBuffer))
        {
          warnUntransformed(attr, "the GPU buffer is not a storage buffer");
          continue;
        }

        const int64_t size = std::min<int64_t>(gpu->byte_size, source->size());
        const int64_t start = in.byte_offset + attr.byte_offset;
        const int64_t stride = g.bindings[attr.binding].byte_stride > 0
                                   ? g.bindings[attr.binding].byte_stride
                                   : int64_t(comps * sizeof(float));
        if(size % 4 != 0 || start < 0 || start % 4 != 0 || stride % 4 != 0
           || stride < 12 || start + 12 > size)
        {
          warnUntransformed(
              attr, "GPU offsets and strides must be multiples of 4 bytes");
          continue;
        }
        int64_t count = (size - start - 12) / stride + 1;
        if(g.vertices > 0)
          count = std::min<int64_t>(count, g.vertices);
        if(!done.emplace(source, start, kind).second)
          continue;

        auto& bake = m_gpuBakes[{port, source}];
        if(!bake.used)
        {
          bake.used = true;
          bake.attributeCount = 0;
          bake.words = size / 4;
          std::copy_n(mats.model, 16, bake.params.model);
          for(int c = 0; c < 3; c++)
          {
            std::copy_n(mats.normal + c * 3, 3, bake.params.normal + c * 4);
            std::copy_n(mats.linear + c * 3, 3, bake.params.linear + c * 4);
            bake.params.normal[c * 4 + 3] = 0.f;
            bake.params.linear[c * 4 + 3] = 0.f;
          }
          sources.push_back(source);
        }
        if(bake.attributeCount >= kMaxGpuBakeAttributes)
        {
          warnUntransformed(attr, "more than 8 attributes share one GPU buffer");
          continue;
        }
        auto* a = bake.params.attributes[bake.attributeCount++];
        a[0] = uint32_t(start / 4);
        a[1] = uint32_t(stride / 4);
        a[2] = uint32_t(count);
        a[3] = uint32_t(kind);
      }
    }

    for(auto* source : sources)
    {
      auto& bake = m_gpuBakes[{port, source}];
      const uint32_t groups
          = uint32_t((bake.words + kGpuBakeLocalSize - 1) / kGpuBakeLocalSize);
      const uint32_t groupsX = std::min(groups, kGpuBakeMaxGroupsX);
      bake.params.counts[0] = uint32_t(bake.words);
      bake.params.counts[1] = uint32_t(bake.attributeCount);
      bake.params.counts[2] = groupsX * kGpuBakeLocalSize;
      bake.params.counts[3] = (groups + groupsX - 1) / groupsX;
      if(!ensureGpuBake(renderer, source, bake))
      {
        bake.attributeCount = 0;
        continue;
      }

      for(auto& g : meshes)
      {
        for(auto& buf : g.buffers)
        {
          auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&buf.data);
          if(gpu && gpu->handle == source)
          {
            gpu->handle = bake.output;
            gpu->byte_size = bake.words * 4;
          }
        }
      }
    }
  }

  void runGpuBakes(
      RenderList& renderer, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res)
  {
    if(!m_gpuBakePipeline || m_gpuBakeFrame == renderer.frame)
      return;
    m_gpuBakeFrame = renderer.frame;

    auto& rhi = *renderer.state.rhi;
    bool any = false;
    for(auto& [key, bake] : m_gpuBakes)
    {
      if(!bake.used || bake.attributeCount == 0 || !bake.srb
         || RenderList::hasRetiredBinding(*bake.srb))
        continue;
      if(!res)
        res = rhi.nextResourceUpdateBatch();
      res->updateDynamicBuffer(bake.ubo, 0, sizeof(GpuBakeParams), &bake.params);
      any = true;
    }
    if(!any)
      return;

    const bool needsComputeBarrier = rhi.backend() == QRhi::OpenGLES2;
    if(needsComputeBarrier)
      commands.beginComputePass(res, QRhiCommandBuffer::BeginPassFlag::ExternalContent);
    else
      commands.beginComputePass(res);
    res = nullptr;

    commands.setComputePipeline(m_gpuBakePipeline);
    for(auto& [key, bake] : m_gpuBakes)
    {
      if(!bake.used || bake.attributeCount == 0 || !bake.srb
         || RenderList::hasRetiredBinding(*bake.srb))
        continue;
      commands.setShaderResources(bake.srb);
      commands.dispatch(bake.params.counts[2] / kGpuBakeLocalSize, bake.params.counts[3], 1);
    }

    if(needsComputeBarrier)
    {
      commands.beginExternal();
      insertComputeBarrier(rhi, commands);
      commands.endExternal();
    }
    commands.endComputePass();
    res = rhi.nextResourceUpdateBatch();
  }

  void rebuild(RenderList& renderer)
  {
    for(auto& [key, bake] : m_gpuBakes)
      bake.used = false;

    auto list = std::make_shared<ossia::mesh_list>();
    auto filters = std::make_shared<ossia::geometry_filter_list>();
    int64_t maxDirty = 0;
    int64_t maxFilterDirty = 0;
    for(int i = 0; i < MergeGeometriesNode::kMaxInputs; ++i)
    {
      const auto* found = findFirstByPort((int32_t)i);
      if(!found || !found->meshes)
      {
        m_cachedInputs[i] = {};
        continue;
      }
      const auto& in = *found;
      if(isIdentity(m_transforms[i]))
      {
        list->meshes.insert(
            list->meshes.end(), in.meshes->meshes.begin(), in.meshes->meshes.end());
      }
      else
      {
        auto baked = bakeCpuAttributes(
            in.meshes->meshes, m_transforms[i],
            [this](const ossia::geometry::attribute& attr, const char* reason) {
              warnUntransformed(attr, reason);
            });
        bakeGpuBuffers(renderer, i, baked, m_transforms[i]);
        list->meshes.insert(
            list->meshes.end(), std::make_move_iterator(baked.begin()),
            std::make_move_iterator(baked.end()));
      }
      m_cachedDirtyIndex[i] = in.meshes->dirty_index;
      maxDirty = std::max(maxDirty, in.meshes->dirty_index);
      if(in.filters)
      {
        filters->filters.insert(
            filters->filters.end(),
            in.filters->filters.begin(),
            in.filters->filters.end());
        maxFilterDirty = std::max(maxFilterDirty, in.filters->dirty_index);
      }
      m_cachedInputs[i] = in;
    }
    list->dirty_index = maxDirty + 1;
    filters->dirty_index = maxFilterDirty + 1;

    m_outputSpec.meshes = std::move(list);
    m_outputSpec.filters = std::move(filters);

    for(auto it = m_gpuBakes.begin(); it != m_gpuBakes.end();)
    {
      if(it->second.used)
      {
        ++it;
        continue;
      }
      releaseGpuBake(renderer, it->second);
      it = m_gpuBakes.erase(it);
    }
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch&, Edge*) override
  {
    if(!m_outputSpec.meshes || this->geometryChanged || m_transformsChanged
       || anyInputChanged())
    {
      rebuild(renderer);
      this->geometryChanged = false;
      m_transformsChanged = false;
    }
  }

  void runInitialPasses(
      RenderList& renderer, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override
  {
    if(!m_outputSpec.meshes)
      return;
    runGpuBakes(renderer, commands, res);
    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    int port_idx = (int)(it - sink->node->input.begin());
    rn_it->second->process(port_idx, m_outputSpec, edge.source);
  }

  void runRenderPass(RenderList&, QRhiCommandBuffer&, Edge&) override { }

  // Data-only renderer — no per-edge GPU pass state to release.
  void removeOutputPass(RenderList&, Edge&) override { }
};

MergeGeometriesNode::MergeGeometriesNode()
{
  for(int i = 0; i < kMaxInputs; ++i)
    input.push_back(new Port{this, {}, Types::Geometry, {}});
  output.push_back(new Port{this, {}, Types::Geometry, {}});
}

MergeGeometriesNode::~MergeGeometriesNode() = default;

NodeRenderer* MergeGeometriesNode::createRenderer(RenderList&) const noexcept
{
  return new RenderedMergeGeometriesNode{*this};
}

}

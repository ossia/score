#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
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
}

std::vector<ossia::geometry> bakeGeometryTransform(
    const std::vector<ossia::geometry>& meshes, const ossia::transform3d& transform)
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
      const int kind = attr.semantic == ossia::attribute_semantic::position  ? 0
                       : attr.semantic == ossia::attribute_semantic::normal  ? 1
                       : attr.semantic == ossia::attribute_semantic::tangent ? 2
                       : attr.semantic == ossia::attribute_semantic::bitangent
                           ? 2
                           : -1;
      if(kind < 0)
        continue;
      const int comps = floatComponents(attr.format);
      if(comps == 0)
        continue;
      if(attr.binding < 0 || attr.binding >= std::ssize(g.input)
         || attr.binding >= std::ssize(g.bindings))
        continue;
      if(g.bindings[attr.binding].classification
         != ossia::geometry::binding::per_vertex)
        continue;
      const auto& in = g.input[attr.binding];
      if(in.buffer < 0 || in.buffer >= std::ssize(g.buffers))
        continue;
      auto& buf = g.buffers[in.buffer];
      auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&buf.data);
      if(!cpu || !cpu->raw_data || cpu->byte_size <= 0)
        continue;

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


struct RenderedMergeGeometriesNode final : NodeRenderer
{
  const MergeGeometriesNode& m_node;
  ossia::geometry_spec m_outputSpec;
  std::array<ossia::geometry_spec, MergeGeometriesNode::kMaxInputs> m_cachedInputs;
  std::array<ossia::transform3d, MergeGeometriesNode::kMaxInputs> m_transforms;
  std::array<int64_t, MergeGeometriesNode::kMaxInputs> m_cachedDirtyIndex{};
  bool m_transformsChanged{};

  RenderedMergeGeometriesNode(const MergeGeometriesNode& n)
      : NodeRenderer{n}
      , m_node{n}
  {
  }

  void init(RenderList&, QRhiResourceUpdateBatch&) override { m_initialized = true; }
  void release(RenderList&) override
  {
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

  void rebuild()
  {
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
        auto baked = bakeGeometryTransform(in.meshes->meshes, m_transforms[i]);
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
  }

  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override
  {
    if(!m_outputSpec.meshes || this->geometryChanged || m_transformsChanged
       || anyInputChanged())
    {
      rebuild();
      this->geometryChanged = false;
      m_transformsChanged = false;
    }
  }

  void runInitialPasses(
      RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      Edge& edge) override
  {
    if(!m_outputSpec.meshes)
      return;
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

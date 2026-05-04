#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <algorithm>

namespace score::gfx
{

struct RenderedMergeGeometriesNode final : NodeRenderer
{
  const MergeGeometriesNode& m_node;
  ossia::geometry_spec m_outputSpec;
  std::array<ossia::geometry_spec, MergeGeometriesNode::kMaxInputs> m_cachedInputs;

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
    m_initialized = false;
  }

  // Since m_portGeometries is now keyed by (port, source), look up the first
  // entry matching the requested port. MergeGeometriesNode wires one input
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
    }
    return false;
  }

  void rebuild()
  {
    auto list = std::make_shared<ossia::mesh_list>();
    int64_t maxDirty = 0;
    for(int i = 0; i < MergeGeometriesNode::kMaxInputs; ++i)
    {
      const auto* found = findFirstByPort((int32_t)i);
      if(!found || !found->meshes)
      {
        m_cachedInputs[i] = {};
        continue;
      }
      const auto& in = *found;
      list->meshes.insert(
          list->meshes.end(),
          in.meshes->meshes.begin(),
          in.meshes->meshes.end());
      maxDirty = std::max(maxDirty, in.meshes->dirty_index);
      m_cachedInputs[i] = in;
    }
    list->dirty_index = maxDirty + 1;

    m_outputSpec.meshes = std::move(list);
    if(!m_outputSpec.filters)
      m_outputSpec.filters = std::make_shared<ossia::geometry_filter_list>();
  }

  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override
  {
    if(!m_outputSpec.meshes || this->geometryChanged || anyInputChanged())
    {
      rebuild();
      this->geometryChanged = false;
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

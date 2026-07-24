#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/detail/hash.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <algorithm>

namespace score::gfx
{

struct RenderedFlattenedSceneFilterNode final : NodeRenderer
{
  const FlattenedSceneFilterNode& m_node;
  ossia::geometry_spec m_outputSpec;
  ossia::geometry_spec m_lastInput;
  int m_lastMode{-1};
  int m_lastMatch{0};
  std::string m_lastMatchStr;

  RenderedFlattenedSceneFilterNode(const FlattenedSceneFilterNode& n)
      : NodeRenderer{n}
      , m_node{n}
  {
  }

  void init(RenderList&, QRhiResourceUpdateBatch&) override { m_initialized = true; }
  void release(RenderList&) override
  {
    m_outputSpec = {};
    m_lastInput = {};
    m_lastMode = -1;
    m_lastMatchStr.clear();
    m_initialized = false;
  }

  bool predicate(
      const ossia::geometry& g, int mode, uint32_t match,
      uint32_t match_str_hash) const noexcept
  {
    switch(mode)
    {
      case 0:  return g.filter_tag == match;
      case 1:  return g.filter_tag != match;
      case 2:  return g.filter_material_index == match;
      case 3:  return g.filter_material_index != match;
      case 4:  return (uint32_t)g.blend == match;
      case 5:  return (uint32_t)g.blend != match;
      case 6:  return g.depth_write == (match != 0);
      case 7:  return g.depth_write != (match != 0);
      case 8:  return (uint32_t)g.cull_mode == match;
      case 9:  return (uint32_t)g.cull_mode != match;
      case 10: return (uint32_t)g.topology == match;
      case 11: return (uint32_t)g.topology != match;
      case 12: return g.filter_tag == match_str_hash;
      case 13: return g.filter_tag != match_str_hash;
      default: return true;
    }
  }

  void rebuild()
  {
    m_outputSpec.meshes = std::make_shared<ossia::mesh_list>();
    m_outputSpec.filters
        = this->geometry.filters
              ? this->geometry.filters
              : std::make_shared<ossia::geometry_filter_list>();

    if(!this->geometry.meshes)
      return;

    const uint32_t matchU = (uint32_t)m_node.m_match;
    // Same hash producers stamp on filter_tag (rapidhash truncated to 32
    // bits). Empty match_str short-circuits to 0u so it matches the
    // "untagged" sentinel rather than rapidhash-of-empty (a non-zero
    // value that would never match anything in practice).
    const uint32_t matchStrHash
        = m_node.m_match_str.empty()
              ? 0u
              : (uint32_t)ossia::hash_string(m_node.m_match_str);
    for(const auto& g : this->geometry.meshes->meshes)
    {
      if(predicate(g, m_node.m_mode, matchU, matchStrHash))
        m_outputSpec.meshes->meshes.push_back(g);
    }
    m_outputSpec.meshes->dirty_index = this->geometry.meshes->dirty_index;
  }

  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override
  {
    const bool geomChanged = (this->geometry != m_lastInput) || this->geometryChanged;
    const bool paramsChanged
        = (m_node.m_mode != m_lastMode) || (m_node.m_match != m_lastMatch)
          || (m_node.m_match_str != m_lastMatchStr);
    if(!geomChanged && !paramsChanged && m_outputSpec.meshes)
      return;

    rebuild();
    m_lastInput = this->geometry;
    m_lastMode = m_node.m_mode;
    m_lastMatch = m_node.m_match;
    m_lastMatchStr = m_node.m_match_str;
    this->geometryChanged = false;
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

FlattenedSceneFilterNode::FlattenedSceneFilterNode()
{
  // Port 0: geometry input
  input.push_back(new Port{this, {}, Types::Geometry, {}});
  // Port 1: filter mode
  {
    auto* data = new int{0};
    input.push_back(new Port{this, data, Types::Int, {}});
  }
  // Port 2: match value (int, modes 0..11)
  {
    auto* data = new int{0};
    input.push_back(new Port{this, data, Types::Int, {}});
  }
  // Port 3: match string (modes 12/13). Carried as a control-only port
  // (no GPU edge type — strings flow through ossia::value via process()
  // rather than as a GPU resource handle).
  {
    auto* data = new std::string{};
    input.push_back(new Port{this, data, Types::Empty, {}});
  }
  output.push_back(new Port{this, {}, Types::Geometry, {}});
}

FlattenedSceneFilterNode::~FlattenedSceneFilterNode() = default;

void FlattenedSceneFilterNode::process(int32_t port, const ossia::value& v)
{
  switch(port)
  {
    case 1:
      m_mode = ossia::convert<int>(v);
      materialChange();
      break;
    case 2:
      m_match = ossia::convert<int>(v);
      materialChange();
      break;
    case 3:
      m_match_str = ossia::convert<std::string>(v);
      materialChange();
      break;
    default:
      ProcessNode::process(port, v);
      break;
  }
}

NodeRenderer*
FlattenedSceneFilterNode::createRenderer(RenderList&) const noexcept
{
  return new RenderedFlattenedSceneFilterNode{*this};
}

}

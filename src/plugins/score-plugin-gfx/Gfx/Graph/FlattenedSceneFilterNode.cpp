#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/detail/hash.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <algorithm>
#include <vector>

namespace score::gfx
{

namespace
{
bool meshPredicate(
    const ossia::geometry& g, int mode, uint32_t match, uint32_t match_str_hash) noexcept
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

bool drawPredicate(
    uint32_t tag, uint32_t material, int mode, uint32_t match,
    uint32_t match_str_hash) noexcept
{
  switch(mode)
  {
    case 0:  return tag == match;
    case 1:  return tag != match;
    case 2:  return material == match;
    case 3:  return material != match;
    case 12: return tag == match_str_hash;
    case 13: return tag != match_str_hash;
    default: return true;
  }
}

// The (tag_hash, material_index) pairs of a merged batch, one per draw command.
const uint32_t* drawFilterKeys(const ossia::geometry& g, std::size_t& count) noexcept
{
  for(const auto& aux : g.auxiliary)
  {
    if(aux.name != "draw_filter_keys" || aux.buffer < 0
       || aux.buffer >= (int)g.buffers.size())
      continue;
    auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&g.buffers[aux.buffer].data);
    if(!cpu || !cpu->raw_data)
      return nullptr;
    count = std::size_t(aux.byte_size) / (2 * sizeof(uint32_t));
    return reinterpret_cast<const uint32_t*>(
        static_cast<const char*>(cpu->raw_data.get()) + aux.byte_offset);
  }
  return nullptr;
}
}

ossia::geometry_spec filterFlattenedScene(
    const ossia::geometry_spec& in, int mode, int match, const std::string& match_str)
{
  ossia::geometry_spec out;
  out.meshes = std::make_shared<ossia::mesh_list>();
  out.filters = in.filters ? in.filters : std::make_shared<ossia::geometry_filter_list>();
  if(!in.meshes)
    return out;

  const uint32_t matchU = (uint32_t)match;
  // Same hash producers stamp on filter_tag (rapidhash truncated to 32
  // bits). Empty match_str short-circuits to 0u so it matches the
  // "untagged" sentinel rather than rapidhash-of-empty (a non-zero
  // value that would never match anything in practice).
  const uint32_t matchStrHash
      = match_str.empty() ? 0u : (uint32_t)ossia::hash_string(match_str);
  const bool perDrawMode
      = mode == 0 || mode == 1 || mode == 2 || mode == 3 || mode == 12 || mode == 13;
  for(const auto& g : in.meshes->meshes)
  {
    std::size_t keyCount = 0;
    const uint32_t* keys = perDrawMode ? drawFilterKeys(g, keyCount) : nullptr;
    if(keys && keyCount == g.cpu_draw_commands.size())
    {
      std::vector<uint32_t> kept;
      for(std::size_t c = 0; c < keyCount; c++)
        if(drawPredicate(keys[2 * c], keys[2 * c + 1], mode, matchU, matchStrHash))
          kept.push_back((uint32_t)c);
      if(kept.empty())
        continue;

      ossia::geometry f = g;
      f.cpu_draw_commands.clear();
      for(uint32_t c : kept)
        f.cpu_draw_commands.push_back(g.cpu_draw_commands[c]);
      // The GPU command lists hold every draw: draw the kept ones from the
      // CPU list instead.
      f.indirect_count = {};
      f.auxiliary.erase(
          std::remove_if(
              f.auxiliary.begin(), f.auxiliary.end(),
              [](const auto& a) { return a.name == "_indirect_draw_count"; }),
          f.auxiliary.end());

      // What indexes the command list follows it: the keys, so that another
      // filter can select among the kept draws, and the alpha-blended run
      // (a range of the list), so that the raster's blend pass draws the kept
      // blended draws and only them.
      constexpr int64_t cmdBytes = 5 * sizeof(uint32_t);
      for(auto it = f.auxiliary.begin(); it != f.auxiliary.end();)
      {
        if(it->name == "draw_filter_keys")
        {
          const int64_t bytes = int64_t(2 * kept.size() * sizeof(uint32_t));
          auto sub = std::shared_ptr<uint32_t[]>(new uint32_t[2 * kept.size()]);
          for(std::size_t k = 0; k < kept.size(); k++)
          {
            sub[2 * k] = keys[2 * kept[k]];
            sub[2 * k + 1] = keys[2 * kept[k] + 1];
          }
          it->buffer = (int)f.buffers.size();
          it->byte_offset = 0;
          it->byte_size = bytes;
          f.buffers.push_back(
              {.data = ossia::geometry::cpu_buffer{
                   std::shared_ptr<void>(sub, sub.get()), bytes}});
        }
        else if(it->name == "_blend_draw_cmds")
        {
          const auto first = uint32_t(it->byte_offset / cmdBytes);
          const auto end = first + uint32_t(it->byte_size / cmdBytes);
          const auto before = std::count_if(
              kept.begin(), kept.end(), [&](uint32_t c) { return c < first; });
          const auto inside = std::count_if(kept.begin(), kept.end(), [&](uint32_t c) {
            return c >= first && c < end;
          });
          if(inside == 0)
          {
            it = f.auxiliary.erase(it);
            continue;
          }
          it->byte_offset = int64_t(before) * cmdBytes;
          it->byte_size = int64_t(inside) * cmdBytes;
        }
        ++it;
      }
      out.meshes->meshes.push_back(std::move(f));
      continue;
    }
    if(meshPredicate(g, mode, matchU, matchStrHash))
      out.meshes->meshes.push_back(g);
  }
  out.meshes->dirty_index = in.meshes->dirty_index;
  return out;
}

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

  void rebuild()
  {
    m_outputSpec
        = filterFlattenedScene(this->geometry, m_node.m_mode, m_node.m_match, m_node.m_match_str);
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
  // Port::value is a non-owning pointer: point it at the members process()
  // writes, rather than at heap storage nothing would ever free.
  // Port 0: geometry input
  input.push_back(new Port{this, {}, Types::Geometry, {}});
  // Port 1: filter mode
  input.push_back(new Port{this, &m_mode, Types::Int, {}});
  // Port 2: match value (int, modes 0..11)
  input.push_back(new Port{this, &m_match, Types::Int, {}});
  // Port 3: match string (modes 12/13). Carried as a control-only port
  // (no GPU edge type — strings flow through ossia::value via process()
  // rather than as a GPU resource handle).
  input.push_back(new Port{this, &m_match_str, Types::Empty, {}});
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

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneFilterNode.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/common/path.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QDebug>

#include <algorithm>
#include <optional>
#include <regex>

namespace score::gfx
{

namespace
{

struct SceneFilterVisitor
{
  int mode{};
  //! Mode 2: the compiled name pattern; empty = no name filtering.
  std::optional<std::regex> name;

  bool matches(const ossia::scene_node& n) const
  {
    return name && std::regex_match(n.name, *name);
  }

  // Mode 2: a matching node is kept whole; otherwise the node stays only as
  // the parent of a kept sub-node, with its transforms and no components.
  ossia::scene_node_ptr keep_matching(const ossia::scene_node_ptr& src) const
  {
    if(!src)
      return nullptr;
    if(matches(*src))
      return src;
    if(!src->has_children())
      return nullptr;
    auto kept = std::make_shared<std::vector<ossia::scene_payload>>();
    bool any_node = false;
    for(const auto& child : *src->children)
    {
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&child))
      {
        if(auto k = keep_matching(*sub))
        {
          kept->push_back(std::move(k));
          any_node = true;
        }
      }
      else if(ossia::get_if<ossia::scene_transform>(&child))
      {
        kept->push_back(child);
      }
    }
    if(!any_node)
      return nullptr;
    auto copy = std::make_shared<ossia::scene_node>(*src);
    copy->children = std::move(kept);
    return copy;
  }

  // Returns true if this payload should be kept in the output tree. When
  // returning true, `out_children` may be populated with rewritten children
  // (for scene_node subtrees that have been partially filtered).
  bool filter_payload(
      const ossia::scene_payload& in, ossia::scene_payload& out) const
  {
    if(auto* n = ossia::get_if<ossia::scene_node_ptr>(&in))
    {
      ossia::scene_node_ptr rewritten = rewrite_node(*n);
      if(!rewritten)
        return false;
      out = rewritten;
      return true;
    }
    // Non-node payloads: pass-through (lights, cameras, materials, meshes,
    // transforms). Hierarchy filtering only drops scene_nodes; payloads
    // carried as direct siblings of a kept node follow their parent.
    out = in;
    return true;
  }

  ossia::scene_node_ptr rewrite_node(const ossia::scene_node_ptr& src) const
  {
    if(!src)
      return nullptr;

    // Mode 1: drop invisible subtrees outright.
    if(mode == 1 && !src->visible)
      return nullptr;

    if(!src->has_children())
    {
      // Leaf node — keep as-is if it passed the visibility check above.
      return src;
    }

    auto newChildren = std::make_shared<std::vector<ossia::scene_payload>>();
    newChildren->reserve(src->children->size());
    for(const auto& child : *src->children)
    {
      ossia::scene_payload out;
      if(filter_payload(child, out))
        newChildren->push_back(std::move(out));
    }

    // If nothing survived under this node, drop the node itself.
    if(newChildren->empty())
      return nullptr;

    // Share-copy: if children were unchanged identity-wise, reuse src.
    if(newChildren->size() == src->children->size())
    {
      bool identical = true;
      for(std::size_t i = 0; i < newChildren->size(); ++i)
      {
        const auto& a = (*newChildren)[i];
        const auto& b = (*src->children)[i];
        if(a.index() != b.index())
        {
          identical = false;
          break;
        }
        // scene_payload is a variant of shared_ptr-to-component types
        // (plus scene_transform). For shared_ptr alternatives, identity
        // is the correct check: a freshly-rewritten subtree returns a
        // different shared_ptr than the original, while pass-through
        // payloads keep the same pointer. scene_transform is always
        // pass-through in filter_payload so equality of the variant
        // index is sufficient — no transform value is mutated here.
        const bool same = ossia::visit(
            [&]<typename T>(const T& av) -> bool {
              const auto* bv = ossia::get_if<T>(&b);
              if(!bv)
                return false;
              if constexpr(requires { av.get() == bv->get(); })
                return av.get() == bv->get();
              else
                return true; // scene_transform: pass-through, treat as same
            },
            a);
        if(!same)
        {
          identical = false;
          break;
        }
      }
      if(identical)
        return src;
    }

    auto copy = std::make_shared<ossia::scene_node>(*src);
    copy->children = std::move(newChildren);
    return copy;
  }

  ossia::scene_spec rewrite(const ossia::scene_spec& in) const
  {
    ossia::scene_spec out;
    if(!in.state)
      return out;

    // Mode 0, or mode 2 without a pattern: pass-through, no copy needed.
    if(mode == 0 || (mode == 2 && !name))
      return in;

    auto newState = std::make_shared<ossia::scene_state>(*in.state);
    auto newRoots
        = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    if(in.state->roots)
    {
      newRoots->reserve(in.state->roots->size());
      for(const auto& r : *in.state->roots)
      {
        if(auto rw = mode == 2 ? keep_matching(r) : rewrite_node(r))
          newRoots->push_back(std::move(rw));
      }
    }
    newState->roots = std::move(newRoots);
    newState->version++;
    newState->dirty_index++;

    out.state = std::move(newState);
    out.delta = in.delta;
    return out;
  }
};

// libossia's OSC pattern matching (ossia::traversal, make_regex): ranges
// expanded, then its character substitutions, anchored on the whole name.
std::optional<std::regex> compileNamePattern(std::string_view pattern)
{
  if(pattern.empty())
    return std::nullopt;
  std::string p{pattern};
  ossia::net::expand_ranges(p);
  try
  {
    return std::regex{"^" + ossia::traversal::substitute_characters(p) + "$"};
  }
  catch(const std::regex_error&)
  {
    qWarning() << "Scene Filter: invalid name pattern" << pattern.data();
    return std::regex{"(?!)"}; // matches nothing
  }
}

}

ossia::scene_spec
filterScene(const ossia::scene_spec& in, int mode, std::string_view pattern)
{
  SceneFilterVisitor vis{mode, mode == 2 ? compileNamePattern(pattern) : std::nullopt};
  return vis.rewrite(in);
}

struct RenderedSceneFilterNode final : NodeRenderer
{
  const SceneFilterNode& m_node;
  ossia::scene_spec m_outputScene;
  const ossia::scene_state* m_cachedInputState{};
  int64_t m_cachedInputVersion{-1};
  int m_cachedMode{-1};
  std::string m_cachedName;

  RenderedSceneFilterNode(const SceneFilterNode& n)
      : NodeRenderer{n}
      , m_node{n}
  {
  }

  void init(RenderList&, QRhiResourceUpdateBatch&) override { m_initialized = true; }
  void release(RenderList&) override
  {
    m_outputScene = {};
    m_cachedInputState = nullptr;
    m_cachedInputVersion = -1;
    m_cachedMode = -1;
    m_cachedName.clear();
    m_initialized = false;
  }

  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override
  {
    const auto* inState = this->scene.state.get();
    const int64_t inVersion = this->scene.state ? this->scene.state->version : -1;

    bool rebuild = !m_outputScene.state
                   || inState != m_cachedInputState
                   || inVersion != m_cachedInputVersion
                   || m_node.m_mode != m_cachedMode
                   || (m_node.m_mode == 2 && m_node.m_name != m_cachedName)
                   || this->sceneChanged;
    if(!rebuild)
      return;

    m_outputScene = filterScene(this->scene, m_node.m_mode, m_node.m_name);
    m_cachedInputState = inState;
    m_cachedInputVersion = inVersion;
    m_cachedMode = m_node.m_mode;
    m_cachedName = m_node.m_name;
    this->sceneChanged = false;
  }

  void runInitialPasses(
      RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      Edge& edge) override
  {
    if(!m_outputScene.state)
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
    rn_it->second->process(port_idx, m_outputScene, edge.source);
  }

  void runRenderPass(RenderList&, QRhiCommandBuffer&, Edge&) override { }

  // Data-only renderer — no per-edge GPU pass state to release.
  void removeOutputPass(RenderList&, Edge&) override { }
};

SceneFilterNode::SceneFilterNode()
{
  input.push_back(new Port{this, {}, Types::Scene, {}});
  // Port::value is non-owning: point it at the member process() writes.
  input.push_back(new Port{this, &m_mode, Types::Int, {}});
  // A control-only port: the string arrives through process().
  input.push_back(new Port{this, &m_name, Types::Empty, {}});
  output.push_back(new Port{this, {}, Types::Scene, {}});
}

SceneFilterNode::~SceneFilterNode() = default;

void SceneFilterNode::process(int32_t port, const ossia::value& v)
{
  switch(port)
  {
    case 1:
      m_mode = ossia::convert<int>(v);
      materialChange();
      break;
    case 2:
      m_name = ossia::convert<std::string>(v);
      materialChange();
      break;
    default:
      ProcessNode::process(port, v);
      break;
  }
}

NodeRenderer* SceneFilterNode::createRenderer(RenderList&) const noexcept
{
  return new RenderedSceneFilterNode{*this};
}

}

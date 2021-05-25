#pragma once
#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <ossia/detail/flat_map.hpp>

#include <score_plugin_gfx_export.h>

#include <algorithm>
#include <optional>
#include <vector>

#include <unordered_map>

namespace score::gfx
{
struct RenderList;
struct Graph;
class GenericNodeRenderer;
class NodeRenderer;

/**
 * @brief Root data model for visual nodes
 */
class SCORE_PLUGIN_GFX_EXPORT Node
{
public:
  explicit Node();
  virtual ~Node();

  virtual NodeRenderer* createRenderer(RenderList& r) const noexcept = 0;
  virtual const Mesh& mesh() const noexcept = 0;

  std::vector<Port*> input;
  ossia::small_pod_vector<Port*, 1> output;
  ossia::flat_map<RenderList*, score::gfx::NodeRenderer*> renderedNodes;

  bool addedToGraph{};
};

/**
 * @brief Common base class for nodes that map to score processes.
 */
class SCORE_PLUGIN_GFX_EXPORT ProcessNode : public Node
{
public:
  using Node::Node;

  int64_t materialChanged{0};
  ProcessUBO standardUBO{};
};

/**
 * @brief Common base class for most single-pass, simple nodes.
 */
class SCORE_PLUGIN_GFX_EXPORT NodeModel : public score::gfx::ProcessNode
{
  friend class GenericNodeRenderer;

public:
  explicit NodeModel();
  virtual ~NodeModel();

  virtual score::gfx::NodeRenderer*
  createRenderer(RenderList& r) const noexcept;

  void setShaders(const QShader& vert, const QShader& frag);

  std::unique_ptr<char[]> m_materialData;

  QShader m_vertexS;
  QShader m_fragmentS;

  friend class GenericNodeRenderer;
};
}

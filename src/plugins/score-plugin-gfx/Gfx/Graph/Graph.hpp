#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/detail/algorithms.hpp>

#include <score_plugin_gfx_export.h>
namespace score::gfx
{
struct OutputNode;
class Window;
/**
 * @brief Represents a graph of renderers.
 */
struct SCORE_PLUGIN_GFX_EXPORT Graph
{
  Graph();
  ~Graph();

  void addNode(score::gfx::Node* n);
  void removeNode(score::gfx::Node* n);

  void addEdge(Port* source, Port* sink);
  void clearEdges();
  void relinkGraph();

  void setVSyncCallback(std::function<void()>);

  std::shared_ptr<RenderList> createRenderer(OutputNode*, RenderState state);

  void setupOutputs(GraphicsApi graphicsApi);
  const std::vector<OutputNode*>& outputs() const noexcept;

private:
  std::vector<std::shared_ptr<RenderList>> m_renderers;
  std::vector<std::shared_ptr<Window>> m_unused_windows;
  std::function<void()> m_vsync_callback;

  std::vector<score::gfx::Node*> m_nodes;
  std::vector<Edge*> m_edges;

  std::vector<OutputNode*> m_outputs;
};
}

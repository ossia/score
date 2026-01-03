#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/detail/algorithms.hpp>

#include <score_plugin_gfx_export.h>
namespace score::gfx
{
class OutputNode;
class Window;
/**
 * @brief Represents a graph of renderers.
 */
struct SCORE_PLUGIN_GFX_EXPORT Graph
{
  /**
   * @brief Create a render graph.
   */
  explicit Graph();

  ~Graph();

  /**
   * @brief Add a node to the graph.
   */
  void addNode(score::gfx::Node* n);

  /**
   * @brief Remove a node from the graph.
   */
  void removeNode(score::gfx::Node* n);

  /**
   * @brief Add an edge between two nodes.
   */
  void addEdge(Port* source, Port* sink);

  /**
   * @brief Remove an edge between two nodes.
   */
  void removeEdge(Port* source, Port* sink);

  /**
   * @brief Add an edge between two nodes and creates relevant pipelines.
   */
  void addAndLinkEdge(Port* source, Port* sink);

  /**
   * @brief Remove an edge between two nodes and free the pipelines
   */
  void unlinkAndRemoveEdge(Port* source, Port* sink);

  /**
   * @brief Remove all edges.
   */
  void clearEdges();

  /**
   * @brief For each output node, create the sequence of render events that will be called.
   */
  void createAllRenderLists(GraphicsApi graphicsApi);

  /**
   * @brief Create a sequence of render events for a single output node
   */
  void createSingleRenderList(score::gfx::OutputNode& node, GraphicsApi graphicsApi);

  /**
   * @brief Free the render list of an output node if any
   */
  void destroyOutputRenderList(score::gfx::OutputNode& node);

  /**
   * @brief Recreate the connections between renderers when edges changed.
   */
  void relinkGraph();

  /**
   * @brief True if the graph supports being driven by the screen vertical synchronization.
   */
  bool canDoVSync() const noexcept;

  const std::vector<std::shared_ptr<RenderList>>& renderLists() const noexcept
  {
    return m_renderers;
  }

  std::span<OutputNode* const> outputs() const noexcept
  {
    return m_outputs;
  }

private:
  void initializeOutput(OutputNode* output, GraphicsApi graphicsApi);
  void createOutputRenderList(OutputNode& output);
  void recreateOutputRenderList(OutputNode& output);
  std::shared_ptr<RenderList>
  createRenderList(OutputNode*, std::shared_ptr<RenderState> state);

  std::vector<std::shared_ptr<RenderList>> m_renderers;
  std::vector<std::shared_ptr<Window>> m_unused_windows;

  std::vector<score::gfx::Node*> m_nodes;
  std::vector<Edge*> m_edges;

  std::vector<OutputNode*> m_outputs;
};
}
